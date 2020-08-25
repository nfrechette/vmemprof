cmake_minimum_required (VERSION 3.2)
project(vmemprof CXX)

set(CMAKE_CXX_STANDARD 11)

# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "No need to run benchmark's tests" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/../external/benchmark" google_benchmark)

include_directories("${PROJECT_SOURCE_DIR}/../external/benchmark/include")

# Grab all of our source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/sources/*.cpp
	${PROJECT_SOURCE_DIR}/sources/*.h)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ALL_TEST_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

setup_default_compiler_flags(${PROJECT_NAME})

# Link Google Benchmark
target_link_libraries(${PROJECT_NAME} PRIVATE benchmark)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
