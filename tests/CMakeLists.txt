cmake_minimum_required(VERSION 3.0)

project(unit_tests C)
enable_testing()

add_executable(message_chunks message_chunks.c CuTest.c)
add_executable(serialization serialization.c CuTest.c)

add_compile_options(-Wall -Wextra)

add_test(message_chunks message_chunks)
add_test(serialization serialization)

target_compile_definitions(serialization PUBLIC NBN_DEBUG)
