cmake_minimum_required(VERSION 3.0)

project(unit_tests C)
enable_testing()

add_compile_options(-Wall -Werror -Wno-unknown-pragmas)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wextra -Wpedantic)
endif (CMAKE_COMPILER_IS_GNUCXX)

# add_executable(message_chunks message_chunks.c CuTest.c)
add_executable(string_tests string_tests.c CuTest.c)
add_executable(serialization_tests serialization.c CuTest.c)

target_link_libraries(string_tests -lm)
target_link_libraries(serialization_tests -lm)

# FIXME: adapt to latest API
# add_test(message_chunks message_chunks)
add_test(serialization_tests serialization_tests)
add_test(string_tests string_tests)

target_compile_definitions(serialization_tests PUBLIC NBN_DEBUG)
