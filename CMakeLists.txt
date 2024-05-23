#
cmake_minimum_required(VERSION 3.16)
project(NekoLib)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option(NEKO_TEST "Activate tests" OFF)

if(NEKO_TEST)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
endif (NEKO_TEST)

add_subdirectory(Core/)