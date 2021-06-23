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

#include <cstdint>
#include <memory>

// We assume a 32-way CPU cache. Because every read we do is at a fixed offset
// from the start of the copy, if we align them to a 4 KB page boundary they will alias.
// This means that the CPU cache will only be able to hold 32 copies.
// However, our DTLB can contain up to 2500 entries this means that when we loop, even though
// our loads miss the L3 cache, the virtual address translation remains cached.
// Each copy we sample requires 3 page entries so we need 2500/3=834 copies.
// To avoid this, use 850 copies.
static void memcpy_tlb(benchmark::State& state)
{
	const size_t PAGE_SIZE = 4 * 1024;
	const size_t SOURCE_SIZE = (17234 + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);		// Round to multiple of PAGE_SIZE
	const size_t NUM_COPIES = state.range(0);
	const size_t INPUT_BUFFER_SIZE = SOURCE_SIZE * NUM_COPIES + PAGE_SIZE;		// Add some padding for alignment

	volatile size_t memcpy_size0 = 401;
	volatile size_t memcpy_size1 = 801;
	volatile size_t memcpy_size2 = 301;

	uint8_t* source_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(source_buffer, 0xA6, SOURCE_SIZE);

	uint8_t* input_buffer = new uint8_t[INPUT_BUFFER_SIZE];
	uint8_t* aligned_input_buffer = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(input_buffer + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
	uint8_t** copies = new uint8_t*[NUM_COPIES];
	for (size_t i = 0; i < NUM_COPIES; ++i)
	{
		uint8_t* buffer = aligned_input_buffer + (i * SOURCE_SIZE);
		std::memcpy(buffer, source_buffer, SOURCE_SIZE);

		copies[i] = buffer;
	}

	uint8_t output_buffer[3 * 1024];
	size_t copy_index = 0;

	for (auto _ : state)
	{
		uint8_t* buffer = copies[copy_index++];

		std::memcpy(output_buffer + 0, buffer + 102, memcpy_size0);
		std::memcpy(output_buffer + memcpy_size0, buffer + 6402, memcpy_size1);
		std::memcpy(output_buffer + memcpy_size0 + memcpy_size1, buffer + 16586, memcpy_size2);

		if (copy_index >= NUM_COPIES)
			copy_index = 0;
	}

	benchmark::DoNotOptimize(input_buffer);
	benchmark::DoNotOptimize(output_buffer);

	delete[] source_buffer;
	delete[] input_buffer;
	delete[] copies;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(double(NUM_COPIES), benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(INPUT_BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// Profiled 3500 entries
// Profiling shows that the L1 TLB miss rate is 2.1%, the L2 miss rate is 26.8%
// The TLB rarely misses
BENCHMARK(memcpy_tlb)->Arg(850)->Arg(1500)->Arg(2500)->Arg(3500)->Repetitions(4);

#endif
