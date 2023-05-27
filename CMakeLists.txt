cmake_minimum_required(VERSION 2.8.12)
project(leptjson_test C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wno-unused-parameter")
endif ()

add_library(leptjson leptjson.c)
add_executable(leptjson_test test.c)
target_compile_options(leptjson_test PRIVATE -gdwarf-4)
target_link_libraries(leptjson_test leptjson)
