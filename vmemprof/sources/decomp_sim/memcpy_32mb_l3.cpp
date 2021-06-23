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

// We assume a 32 MB CPU cache, add a bit extra, and allocate a single 33MB buffer
// that contains our original data duplicated as many times as it fits. We then
// iterate over this and loop back. This ensures our CPU cache is always cold.
static void memcpy_32mb_l3(benchmark::State& state)
{
	const size_t L3_SIZE = state.range(0) * 1024 * 1024;
	const size_t SOURCE_SIZE = 17234;
	const size_t TOTAL_COPY_SIZE = 401 + 801 + 301;
	const size_t NUM_COPIES = L3_SIZE / TOTAL_COPY_SIZE;
	const size_t INPUT_BUFFER_SIZE = SOURCE_SIZE * NUM_COPIES;

	volatile size_t memcpy_size0 = 401;
	volatile size_t memcpy_size1 = 801;
	volatile size_t memcpy_size2 = 301;

	uint8_t* source_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(source_buffer, 0xA6, SOURCE_SIZE);

	uint8_t* input_buffer = new uint8_t[INPUT_BUFFER_SIZE];
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

// Profiling shows that the L1 TLB miss rate is 1.7%, the L2 miss rate is 27.9%
// The TLB rarely misses
// Timings are about the same and about 3.96585G/s - 4.05808G/s
BENCHMARK(memcpy_32mb_l3)->Arg(33)->Arg(43)->Arg(53)->Repetitions(4);
