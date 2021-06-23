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

#include <chrono>
#include <cstdint>
#include <memory>

static void memset_impl(uint8_t* buffer, size_t buffer_size, uint8_t value)
{
	for (uint8_t* ptr = buffer; ptr < buffer + buffer_size; ++ptr)
		*ptr = value;
}

// We allocate a single buffer 4x the size of the CPU cache and write to it
// to make sure we evict all our CPU cache with a custom memset.
// We allocate 1000 copies of the source buffer and align them to reduce the flush cost
// by flushing only when we loop around.
static void memcpy_cpu_flush(benchmark::State& state)
{
	const size_t SOURCE_SIZE = 17234;
	const size_t CACHE_LINE_SIZE = 64;
	const size_t FLUSH_BUFFER_SIZE = state.range(0) * 1024 * 1024 * 4;
	const size_t PAGE_SIZE = 4 * 1024;
	const size_t NUM_COPIES = 1000;
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

	uint8_t* flush_buffer = new uint8_t[FLUSH_BUFFER_SIZE];
	uint8_t flush_value = 0;

	uint8_t output_buffer[3 * 1024];
	size_t copy_index = 0;

	// Flush the CPU cache
	memset_impl(flush_buffer, FLUSH_BUFFER_SIZE, flush_value++);

	for (auto _ : state)
	{
		uint8_t* buffer = copies[copy_index++];

		const auto start = std::chrono::high_resolution_clock::now();

		std::memcpy(output_buffer + 0, buffer + 102, memcpy_size0);
		std::memcpy(output_buffer + memcpy_size0, buffer + 6402, memcpy_size1);
		std::memcpy(output_buffer + memcpy_size0 + memcpy_size1, buffer + 16586, memcpy_size2);

		const auto end = std::chrono::high_resolution_clock::now();

		if (copy_index >= NUM_COPIES)
		{
			// Flush the CPU cache
			memset_impl(flush_buffer, FLUSH_BUFFER_SIZE, flush_value++);

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
	state.counters["Allocated"] = benchmark::Counter(double(FLUSH_BUFFER_SIZE + INPUT_BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

BENCHMARK(memcpy_cpu_flush)->Arg(8)->Arg(16)->Arg(32)->Repetitions(4)->UseManualTime();
