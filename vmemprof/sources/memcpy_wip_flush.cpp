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
#include <random>
#include <thread>

#ifdef _WIN32
	#include <windows.h>
#endif

static void memset_impl(uint8_t* buffer, size_t buffer_size, uint8_t value)
{
#if 0
	// Make sure any VMEM prefetching happens towards the middle of our buffer to avoid spilling on either end
	const size_t lhs = buffer_size / 2;
	const size_t rhs = buffer_size - lhs;
	for (uint8_t* ptr = buffer; ptr < buffer + lhs; ++ptr)
		*ptr = value;
	for (uint8_t* ptr = buffer + buffer_size - 1; ptr >= buffer + lhs; --ptr)
		*ptr = value;
#elif 0
	const size_t lhs = buffer_size / 2;
	const size_t rhs = buffer_size - lhs;
	for (uint8_t* ptr = buffer; ptr < buffer + lhs; ++ptr)
		*ptr = value;
	for (uint8_t* ptr = buffer + lhs; ptr < buffer + buffer_size; ++ptr)
		*ptr = value;
#elif 0
	std::memset(buffer, value, buffer_size);	// memset uses non-temporal writes which bypass the CPU cache
#else
	for (uint8_t* ptr = buffer; ptr < buffer + buffer_size; ++ptr)
		*ptr = value;
#endif

	//std::this_thread::sleep_for(std::chrono::nanoseconds(1));
	//_mm_mfence();
	//_mm_lfence();
}

// We allocate a single buffer 4x the size of the CPU cache and write to it
// to make sure we evict all our CPU and TLB cache.
// We allocate 1000 copies of the source buffer and align them to reduce the flush cost
// by flushing only when we loop around.
static void memcpy_wip_flush(benchmark::State& state)
{
	const size_t SOURCE_SIZE = 17234;
	const size_t CACHE_LINE_SIZE = 64;
	const size_t FLUSH_BUFFER_SIZE = state.range(0) * 1024 * 1024 * 4;
	const size_t PAGE_SIZE = 4 * 1024;
	const size_t PADDED_SOURCE_SIZE = (SOURCE_SIZE + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);		// Round to multiple of PAGE_SIZE
	const size_t NUM_COPIES = 1000;
	const size_t INPUT_BUFFER_SIZE = SOURCE_SIZE * NUM_COPIES + PAGE_SIZE;		// Add some padding for alignment
	const size_t ONE_GB = 1 * 1024 * 1024 * 1024;
	const size_t SCRATCH_BUFFER_SIZE = 2 * ONE_GB;

	volatile size_t memcpy_size0 = 401;
	volatile size_t memcpy_size1 = 801;
	volatile size_t memcpy_size2 = 301;

	uint8_t* source_buffer = new uint8_t[SOURCE_SIZE];
	std::memset(source_buffer, 0xA6, SOURCE_SIZE);

	uint8_t* scratch_buffer = new uint8_t[SCRATCH_BUFFER_SIZE];
	uint8_t* aligned_scratch_buffer = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(scratch_buffer + ONE_GB - 1) & ~(ONE_GB - 1));
	std::memset(aligned_scratch_buffer, 0, ONE_GB);

	//uint8_t* input_buffer = new uint8_t[INPUT_BUFFER_SIZE];
	//uint8_t* aligned_input_buffer = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(input_buffer + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
	uint8_t* aligned_input_buffer = aligned_scratch_buffer;
	uint8_t** copies = new uint8_t*[NUM_COPIES];
	for (size_t i = 0; i < NUM_COPIES; ++i)
	{
		uint8_t* buffer = aligned_input_buffer + (i * SOURCE_SIZE);
		std::memcpy(buffer, source_buffer, SOURCE_SIZE);

		copies[i] = buffer;
	}

	//std::shuffle(copies, copies + NUM_COPIES, std::default_random_engine(42));

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

	//const size_t VMEM_PADDING = 8 * 1024 * 1024;
	const size_t VMEM_PADDING = state.range(1) * 1024 * 1024;
	const size_t PADDED_FLUSH_BUFFER_SIZE = FLUSH_BUFFER_SIZE + VMEM_PADDING * 2;
	//uint8_t* flush_buffer = new uint8_t[PADDED_FLUSH_BUFFER_SIZE];
	uint8_t* flush_buffer =  aligned_scratch_buffer + (ONE_GB / 2);
	uint8_t flush_value = 0;

	uint8_t output_buffer[3 * 1024];
	size_t copy_index = 0;

	// Flush our TLB (but not the CPU cache entries containing translation mappings)
	//VirtualProtect(aligned_input_buffer, INPUT_BUFFER_SIZE - PAGE_SIZE, PAGE_NOACCESS, nullptr);
	//VirtualProtect(aligned_input_buffer, INPUT_BUFFER_SIZE - PAGE_SIZE, PAGE_READWRITE, nullptr);
	VirtualProtect(aligned_scratch_buffer, ONE_GB, PAGE_NOACCESS, nullptr);
	VirtualProtect(aligned_scratch_buffer, ONE_GB, PAGE_READWRITE, nullptr);

	// Flush the CPU cache
	//std::memset(flush_buffer + VMEM_PADDING, flush_value++, FLUSH_BUFFER_SIZE);
	memset_impl(flush_buffer + VMEM_PADDING, FLUSH_BUFFER_SIZE, flush_value++);

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
			// Flush our TLB (but not the CPU cache entries containing translation mappings)
			//VirtualProtect(aligned_input_buffer, INPUT_BUFFER_SIZE - PAGE_SIZE, PAGE_NOACCESS, nullptr);
			//VirtualProtect(aligned_input_buffer, INPUT_BUFFER_SIZE - PAGE_SIZE, PAGE_READWRITE, nullptr);
			VirtualProtect(aligned_scratch_buffer, ONE_GB, PAGE_NOACCESS, nullptr);
			VirtualProtect(aligned_scratch_buffer, ONE_GB, PAGE_READWRITE, nullptr);

			// Flush the CPU cache
			//std::memset(flush_buffer + VMEM_PADDING, flush_value++, FLUSH_BUFFER_SIZE);
			memset_impl(flush_buffer + VMEM_PADDING, FLUSH_BUFFER_SIZE, flush_value++);

			copy_index = 0;
		}

		const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		state.SetIterationTime(elapsed_seconds.count());
	}

	//benchmark::DoNotOptimize(input_buffer);
	benchmark::DoNotOptimize(scratch_buffer);
	benchmark::DoNotOptimize(output_buffer);
	benchmark::DoNotOptimize(flush_buffer);

	delete[] source_buffer;
	//delete[] input_buffer;
	delete[] copies;
	//delete[] flush_buffer;
	delete[] scratch_buffer;

	state.counters["Speed"] = benchmark::Counter(1503, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
	state.counters["NumCopies"] = benchmark::Counter(NUM_COPIES, benchmark::Counter::kDefaults, benchmark::Counter::kIs1000);
	state.counters["Allocated"] = benchmark::Counter(double(PADDED_FLUSH_BUFFER_SIZE + INPUT_BUFFER_SIZE), benchmark::Counter::kDefaults, benchmark::Counter::kIs1024);
}

// Size of cache line padding doesn't seem to matter much, within noise measurement margin
// TODO: Measure without hyperthread noise, pin to cpu
// TODO: Add padding between copies, randomize them to ensure no prefetching of VMEM translation entries

// Profiling shows that the L1 TLB miss rate is 1.3%, the L2 miss rate is 21.9%
//memcpy_cpu_flush / 32 / iterations : 3000000 / repeats : 4 / manual_time_mean          147 ns         6098 ns            4 Allocated = 144.44M NumCopies = 1000 Speed = 9.66342G / s
//memcpy_cpu_flush / 32 / iterations : 3000000 / repeats : 4 / manual_time_median        146 ns         6099 ns            4 Allocated = 144.44M NumCopies = 1000 Speed = 9.62678G / s
//memcpy_cpu_flush / 32 / iterations : 3000000 / repeats : 4 / manual_time_stddev       19.8 ns         38.4 ns            4 Allocated = 0 NumCopies = 0 Speed = 1.2956G / s
//BENCHMARK(memcpy_wip_flush)->Arg(8)->Arg(16)->Arg(32)->Iterations(3000000)->Repetitions(4)->UseManualTime();
//BENCHMARK(memcpy_wip_flush)->Args({ 8, 0 })->Args({ 8, 2 })->Args({ 8, 8 })->Args({ 8, 32 })->Args({ 8, 64 })->Args({ 8, 96 })->Args({ 32, 0 })->Args({ 32, 2 })->Args({ 32, 8 })->Args({ 32, 32 })->Args({ 32, 64 })->Args({ 32, 96 })->Iterations(3000000)->Repetitions(4)->UseManualTime();
