#
cmake_minimum_required(VERSION 3.16)


set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_USE_MATH_DEFINES)

option(NEKO_TEST "Activate tests" OFF)
option(NEKO_BENCHMARK "Activate benchmarks" OFF)
option(NEKO_SAMPLES "Activate samples" OFF)
option(ENABLE_PROFILING "Activate profiling" OFF)

if(NEKO_TEST)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif (NEKO_TEST)

if(NEKO_BENCHMARK)
    list(APPEND VCPKG_MANIFEST_FEATURES "benchmark")
endif (NEKO_BENCHMARK)

if(ENABLE_PROFILING)
    list(APPEND VCPKG_MANIFEST_FEATURES "profile")
endif(ENABLE_PROFILING)
project(NekoLib)


if(NEKO_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
endif (NEKO_TEST)

if(NEKO_BENCHMARK)
    find_package(benchmark CONFIG REQUIRED)
endif (NEKO_BENCHMARK)

if(ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
endif(ENABLE_PROFILING)
add_subdirectory(Core/)