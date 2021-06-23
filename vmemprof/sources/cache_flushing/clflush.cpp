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

// We can use the _mm_clflush intrinsic to flush our CPU cache
static void cache_flushing_clflush(benchmark::State& state)
{
	constexpr size_t BUFFER_SIZE = 17234;
	constexpr size_t CACHE_LINE_SIZE = 64;

	volatile size_t memcpy_size = 401;

	uint8_t* input_buffer = new uint8_t[BUFFER_SIZE];
	std::memset(input_buffer, 0xA6, BUFFER_SIZE);

	uint8_t output_buffer[3 * 1024];

	for (auto _ : state)
	{
		const auto start = std::chrono::high_resolution_clock::now();

		std::memcpy(output_buffer + 0, input_buffer + 102, memcpy_size);

		const auto end = std::chrono::high_resolution_clock::now();

		// Flush every cache line we use
		for (const uint8_t* ptr = input_buffer; ptr < input_buffer + BUFFER_SIZE; ptr += CACHE_LINE_SIZE)
			_mm_clflush(ptr);

		const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		state.SetIterationTime(elapsed_seconds.count());
	}

	benchmark::DoNotOptimize(input_buffer);
	benchmark::DoNotOptimize(output_buffer);

	delete[] input_buffer;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(1, benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// Profiling shows that the L1 TLB miss rate is 0.9%, the L2 miss rate is 20.8%
// The TLB rarely misses
BENCHMARK(cache_flushing_clflush)->Iterations(100000)->Repetitions(4)->UseManualTime();

#endif
