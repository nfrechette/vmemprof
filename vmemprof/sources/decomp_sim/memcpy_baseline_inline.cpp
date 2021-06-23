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

#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>

// Naive memcpy with statically known sizes, the compiler is likely to inline it
static void memcpy_baseline_inline(benchmark::State& state)
{
	const size_t SOURCE_SIZE = 17234;

	const size_t memcpy_size0 = 401;
	const size_t memcpy_size1 = 801;
	const size_t memcpy_size2 = 301;

	uint8_t* input_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(input_buffer, 0xA6, SOURCE_SIZE);

	uint8_t output_buffer[3 * 1024];

	for (auto _ : state)
	{
		std::memcpy(output_buffer + 0, input_buffer + 102, memcpy_size0);
		std::memcpy(output_buffer + memcpy_size0, input_buffer + 6402, memcpy_size1);
		std::memcpy(output_buffer + memcpy_size0 + memcpy_size1, input_buffer + 16586, memcpy_size2);
	}

	benchmark::DoNotOptimize(input_buffer);
	benchmark::DoNotOptimize(output_buffer);

	delete[] input_buffer;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(1, benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(SOURCE_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

BENCHMARK(memcpy_baseline_inline)->Repetitions(4);
