cmake_minimum_required(VERSION 3.0)

project(unit_tests C)
enable_testing()

add_executable(message_chunks message_chunks.c CuTest.c)

add_test(message_chunks message_chunks)

target_compile_definitions(message_chunks PUBLIC NBN_DEBUG)
