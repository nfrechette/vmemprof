////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "decomp_sim.h"

#if defined(DECOMP_SIM_ENABLED)

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstdint>
#include <memory>

#ifdef _WIN32
	#include <windows.h>
#endif

// For better control, allocate the virtual memory manually.
// During virtual address translation, the last level of the page table
// has entries that each cover 4KB (1 page). A total of 9 bits are used
// from the virtual address to offset into so the maximum range of that
// is 2 MB. This means when we translate the virtual memory address,
// we'll load a cache line with offsets to 4 KB pages. Each cache line
// contains 8 entries and thus covers at most 32 KB. We'll round up to 40 KB (10 pages)
// to make sure there is no overlap and space our copies by that much
// to ensure each time we touch a copy a new cache miss is also triggered
// during address translation.
// We'll use only 32 copies since it should be enough due to aliasing.
// Even though we have a lot of spacing, it doesn't help us with the TLB and
// the cached page table entries. To force the TLB to flush, we'll change the
// access rights of the memory region we allocate and restore them. This will
// force the kernel to evict the TLB entries for our memory range but it will
// not evict the page table entries from the CPU cache.
static void memcpy_vmem_tlb_flush(benchmark::State& state)
{
	const size_t PAGE_SIZE = 4 * 1024;
	const size_t PADDING_SIZE = state.range(0) * 1024;
	const size_t SOURCE_SIZE = (17234 + PADDING_SIZE - 1) / PADDING_SIZE;				// Round to multiple of PAGE_SIZE
	const size_t NUM_COPIES = 32;
	const size_t INPUT_BUFFER_SIZE = SOURCE_SIZE * NUM_COPIES + PADDING_SIZE;			// Add some padding for alignment

	volatile size_t memcpy_size0 = 401;
	volatile size_t memcpy_size1 = 801;
	volatile size_t memcpy_size2 = 301;

	uint8_t* source_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(source_buffer, 0xA6, SOURCE_SIZE);

	uint8_t* input_buffer = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, INPUT_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	uint8_t** copies = new uint8_t*[NUM_COPIES];
	for (size_t i = 0; i < NUM_COPIES; ++i)
	{
		uint8_t* buffer = input_buffer + (i * SOURCE_SIZE);
		std::memcpy(buffer, source_buffer, SOURCE_SIZE);

		copies[i] = buffer;
	}

	uint8_t output_buffer[3 * 1024];
	size_t copy_index = 0;

	for (auto _ : state)
	{
		const auto start = std::chrono::high_resolution_clock::now();

		uint8_t* buffer = copies[copy_index++];

		std::memcpy(output_buffer + 0, buffer + 102, memcpy_size0);
		std::memcpy(output_buffer + memcpy_size0, buffer + 6402, memcpy_size1);
		std::memcpy(output_buffer + memcpy_size0 + memcpy_size1, buffer + 16586, memcpy_size2);

		const auto end = std::chrono::high_resolution_clock::now();

		if (copy_index >= NUM_COPIES)
		{
			copy_index = 0;

			VirtualProtect(input_buffer, INPUT_BUFFER_SIZE, PAGE_NOACCESS, nullptr);
			VirtualProtect(input_buffer, INPUT_BUFFER_SIZE, PAGE_READWRITE, nullptr);
		}

		const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		state.SetIterationTime(elapsed_seconds.count());
	}

	benchmark::DoNotOptimize(input_buffer);
	benchmark::DoNotOptimize(output_buffer);

	delete[] source_buffer;
	VirtualFree(input_buffer, INPUT_BUFFER_SIZE, MEM_RELEASE);
	delete[] copies;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(double(NUM_COPIES), benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(INPUT_BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// Profiled 40 entries
// Profiling shows that the L1 TLB miss rate is 0.0%, the L2 miss rate is 0.3%
// It looks like we manage to flush the TLB but we still hit the L1 regardless.
BENCHMARK(memcpy_vmem_tlb_flush)->Arg(40)->Arg(68)->Repetitions(4)->UseManualTime();

#endif
