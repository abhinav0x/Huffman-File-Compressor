cmake_minimum_required(VERSION 3.10)
project(HuffmanCoder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(huffman huffman.cpp)

if (MSVC)
    target_compile_options(huffman PRIVATE /W4)
else()
    target_compile_options(huffman PRIVATE -Wall -Wextra -O2)
endif()