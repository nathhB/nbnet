cmake_minimum_required(VERSION 3.5)

project(unit_tests C)
enable_testing()

add_compile_options(-Wall)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(-Wextra -Wpedantic -Werror -Wno-unknown-pragmas)
endif (CMAKE_COMPILER_IS_GNUCXX)

# add_executable(message_chunks message_chunks.c CuTest.c)
add_executable(string_tests string_tests.c CuTest.c)

target_link_libraries(string_tests -lm)

add_test(string_tests string_tests)
