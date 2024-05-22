
file(GLOB_RECURSE INCLUDE_FILES include/*.h)
file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_library(NekoCore ${INCLUDE_FILES} ${SRC_FILES})

