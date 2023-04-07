cmake_minimum_required(VERSION 3.0)

project(unit_tests C)
enable_testing()

# add_executable(message_chunks message_chunks.c CuTest.c)
add_executable(string_tests string_tests.c CuTest.c)
add_executable(serialization_tests serialization.c CuTest.c)

add_compile_options(-Wall -Wextra)

# add_test(message_chunks message_chunks)
add_test(serialization_tests serialization_tests)
add_test(string_tests string_tests)

target_compile_definitions(serialization_tests PUBLIC NBN_DEBUG)
