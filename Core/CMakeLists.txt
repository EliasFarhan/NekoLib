
file(GLOB_RECURSE INCLUDE_FILES include/*.h)
file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_library(NekoCore ${INCLUDE_FILES} ${SRC_FILES})
target_include_directories(NekoCore PUBLIC include/)

if(NEKO_TEST)
    file(GLOB_RECURSE TEST_FILES test/*.cpp)
    add_executable(NekoCoreTest ${TEST_FILES})
    target_link_libraries(NekoCoreTest NekoCore GTest::gtest GTest::gtest_main)
    add_test(CoreTest NekoCoreTest)
endif (NEKO_TEST)

if(NEKO_BENCHMARK)
    file(GLOB_RECURSE BENCH_FILES bench/*.cpp)
    foreach (BENCH_FILE ${BENCH_FILES})
        get_filename_component(EXE_NAME ${BENCH_FILE} NAME_WE)
        add_executable(${EXE_NAME} ${BENCH_FILE})
        target_link_libraries(${EXE_NAME} NekoCore benchmark::benchmark)
    endforeach ()
endif (NEKO_BENCHMARK)