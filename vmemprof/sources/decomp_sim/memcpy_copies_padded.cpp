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

static void memset_impl(uint8_t* buffer, size_t buffer_size, uint8_t value)
{
	for (uint8_t* ptr = buffer; ptr < buffer + buffer_size; ++ptr)
		*ptr = value;
}

static void memcpy_impl(uint8_t* dst, const uint8_t* src, size_t count)
{
	const uint8_t* end = src + count;
	while (src < end)
	{
		*dst++ = *src++;
	}
}

// We allocate a single buffer 4x the size of the CPU cache and write to it
// to make sure we evict all our CPU cache with a custom memset.
// Our flush buffer has padding before/after to guard against VMEM translation prefetching.
// We allocate 1000 copies of the source buffer and align them to reduce the flush cost
// by flushing only when we loop around. We pad each copy to 16 MB to ensure no VMEM entry sharing in L2.
static void memcpy_copies_padded(benchmark::State& state)
{
	const size_t SOURCE_SIZE = 17234;
	const size_t CPU_CACHE_SIZE = 8 * 1024 * 1024;
	const size_t FLUSH_BUFFER_SIZE = CPU_CACHE_SIZE * 4;
	//const size_t PAGE_SIZE = 4 * 1024;

	const size_t SOURCE_PADDING_SIZE = state.range(0);
	size_t PADDED_SOURCE_SIZE;
	if (SOURCE_PADDING_SIZE != 0)
		PADDED_SOURCE_SIZE = (SOURCE_SIZE + SOURCE_PADDING_SIZE - 1) & ~(SOURCE_PADDING_SIZE - 1);		// Round to multiple of SOURCE_PADDING_SIZE
	else
		PADDED_SOURCE_SIZE = SOURCE_SIZE;		// No padding

	const size_t NUM_COPIES = 1000;
	//const size_t INPUT_BUFFER_ALIGNMENT = SOURCE_PADDING_SIZE != 0 ? SOURCE_PADDING_SIZE : PAGE_SIZE;
	const size_t INPUT_BUFFER_ALIGNMENT = 2 * 1024 * 1024;
	const size_t INPUT_BUFFER_SIZE = PADDED_SOURCE_SIZE * NUM_COPIES + INPUT_BUFFER_ALIGNMENT;		// Add some padding for alignment

	volatile size_t memcpy_size0 = 401;
	volatile size_t memcpy_size1 = 801;
	volatile size_t memcpy_size2 = 301;
	volatile size_t memcpy_size3 = 501;	// ???

	uint8_t* source_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(source_buffer, 0xA6, SOURCE_SIZE);

	uint8_t* input_buffer = new uint8_t[INPUT_BUFFER_SIZE];
	uint8_t* aligned_input_buffer = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(input_buffer + INPUT_BUFFER_ALIGNMENT - 1) & ~(INPUT_BUFFER_ALIGNMENT - 1));
	uint8_t** copies = new uint8_t*[NUM_COPIES];
	for (size_t i = 0; i < NUM_COPIES; ++i)
	{
		uint8_t* buffer = aligned_input_buffer + (i * PADDED_SOURCE_SIZE);
		std::memcpy(buffer, source_buffer, SOURCE_SIZE);

		copies[i] = buffer;
	}

	// The VMEM Level 1 translation has 512 entries each spanning 1 GB. We'll assume that in the real world
	// there is a reasonable chance that memory touched will live within the same 1 GB region and thus be
	// in some level of the CPU cache.

	// The VMEM Level 2 translation has 512 entries each spanning 2 MB.
	// This means the cache line we load to find a page offset contains a span of 16 MB within it (a cache
	// line contains 8 entries).
	// To ensure we don't touch cache lines that belong to our input buffer as we flush the CPU cache,
	// we add sufficient padding at both ends of the flush buffer. Since we'll access it linearly,
	// the hardware prefetcher might pull in cache lines ahead. We assume it won't pull more than 4 cache
	// lines ahead. This means we need this much padding on each end: 4 * 16 MB = 64 MB

	const size_t VMEM_PADDING = 16 * 1024 * 1024;
	const size_t PADDED_FLUSH_BUFFER_SIZE = FLUSH_BUFFER_SIZE + VMEM_PADDING * 2;
	uint8_t* flush_buffer = new uint8_t[PADDED_FLUSH_BUFFER_SIZE];
	uint8_t flush_value = 0;

	uint8_t output_buffer[3 * 1024];
	size_t copy_index = 0;

	// Flush the CPU cache
	memset_impl(flush_buffer + VMEM_PADDING, FLUSH_BUFFER_SIZE, flush_value++);

	for (auto _ : state)
	{
		uint8_t* buffer = copies[copy_index++];

		const auto start = std::chrono::high_resolution_clock::now();

		//std::memcpy(output_buffer + 0, buffer + 102, memcpy_size0);
		//std::memcpy(output_buffer + memcpy_size0, buffer + 6402, memcpy_size1);
		//std::memcpy(output_buffer + memcpy_size0 + memcpy_size1 + memcpy_size2, buffer + 12308, memcpy_size3);	// ???
		//std::memcpy(output_buffer + memcpy_size0 + memcpy_size1, buffer + 16586, memcpy_size2);

		memcpy_impl(output_buffer + 0, buffer + 102, memcpy_size0);
		memcpy_impl(output_buffer + memcpy_size0, buffer + 6402, memcpy_size1);
		memcpy_impl(output_buffer + memcpy_size0 + memcpy_size1 + memcpy_size2, buffer + 12308, memcpy_size3);	// ???
		memcpy_impl(output_buffer + memcpy_size0 + memcpy_size1, buffer + 16586, memcpy_size2);

		const auto end = std::chrono::high_resolution_clock::now();

		if (copy_index >= NUM_COPIES)
		{
			// Flush the CPU cache
			memset_impl(flush_buffer + VMEM_PADDING, FLUSH_BUFFER_SIZE, flush_value++);

			copy_index = 0;
		}

		const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		state.SetIterationTime(elapsed_seconds.count());
	}

	benchmark::DoNotOptimize(input_buffer);
	benchmark::DoNotOptimize(output_buffer);
	benchmark::DoNotOptimize(flush_buffer);

	delete[] source_buffer;
	delete[] input_buffer;
	delete[] copies;
	delete[] flush_buffer;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(NUM_COPIES, benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(PADDED_FLUSH_BUFFER_SIZE + INPUT_BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// 16MB is the minimum value where the variance reduces consistently
// TODO: Round to cache line multiple for all read/writes and sizes to make sure the number of cache lines touched doesn't depend on alignment
// Right now, 16mb alignment is indeed consistently slower than 4kb, as expected. But having no alignment, is slower still. Either it prefetches
// more poorly or it loads more cache lines.
BENCHMARK(memcpy_copies_padded)->Arg(0)->Arg(4 * 1024)->Arg(16 * 1024 * 1024)->Repetitions(4)->UseManualTime();

#endif
