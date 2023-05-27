cmake_minimum_required(VERSION 3.0.0)
project(leptjson_test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -pedantic -Wall -Wno-unused-parameter")
endif ()

add_library(leptjson leptjson.c)
add_executable(leptjson_test test.c)
target_compile_options(leptjson PRIVATE -gdwarf-4)
target_compile_options(leptjson_test PRIVATE -gdwarf-4)
target_link_libraries(leptjson_test leptjson)
