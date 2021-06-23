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

#include "cache_flushing.h"

#if defined(CACHE_FLUSHING_ENABLED)

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstdint>
#include <memory>

// We allocate a single buffer 4x the size of the CPU cache and write to it
// to make sure we evict all our CPU cache with std::memset.
// We allocate 1000 copies of the source buffer and align them to reduce the flush cost
// by flushing only when we loop around.
static void cache_flushing_flush_std_memset(benchmark::State& state)
{
	constexpr size_t SOURCE_SIZE = 17234;
	constexpr size_t PAGE_SIZE = 4 * 1024;
	constexpr size_t NUM_COPIES = 1000;
	constexpr size_t INPUT_BUFFER_SIZE = SOURCE_SIZE * NUM_COPIES + PAGE_SIZE;		// Add some padding for alignment

	volatile size_t memcpy_size = 401;

	// Some source buffer we'll copy
	uint8_t* source_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(source_buffer, 0xA6, SOURCE_SIZE);

	// Actual buffer used by benchmark with multiple copies of our source
	uint8_t* input_buffer = new uint8_t[INPUT_BUFFER_SIZE];
	uint8_t* aligned_input_buffer = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(input_buffer + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
	uint8_t* copies[NUM_COPIES];
	for (size_t i = 0; i < NUM_COPIES; ++i)
	{
		uint8_t* buffer = aligned_input_buffer + (i * SOURCE_SIZE);
		std::memcpy(buffer, source_buffer, SOURCE_SIZE);

		copies[i] = buffer;
	}

	// Buffer we use for flushing
	const size_t cache_size = state.range(0) * 1024 * 1024;	// Convert MB to bytes
	const size_t flush_buffer_size = cache_size * 4;		// 4x larger
	uint8_t* flush_buffer = new uint8_t[flush_buffer_size];
	uint8_t flush_value = 0;

	// Output buffer we write to
	uint8_t output_buffer[3 * 1024];
	size_t copy_index = 0;

	// Flush the CPU cache
	std::memset(flush_buffer, flush_value++, flush_buffer_size);

	for (auto _ : state)
	{
		uint8_t* buffer = copies[copy_index++];

		const auto start = std::chrono::high_resolution_clock::now();

		std::memcpy(output_buffer + 0, buffer + 102, memcpy_size);

		const auto end = std::chrono::high_resolution_clock::now();

		if (copy_index >= NUM_COPIES)
		{
			// Flush the CPU cache
			std::memset(flush_buffer, flush_value++, flush_buffer_size);

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
	delete[] flush_buffer;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(NUM_COPIES, benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(flush_buffer_size + INPUT_BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

BENCHMARK(cache_flushing_flush_std_memset)->Arg(8)->Arg(16)->Arg(32)->Iterations(100000)->Repetitions(4)->UseManualTime();

#endif
