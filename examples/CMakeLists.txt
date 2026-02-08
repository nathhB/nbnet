cmake_minimum_required(VERSION 3.5)
project(nbnet_examples)

option(EXAMPLES_UDP OFF)

if (EXAMPLES_UDP)
    set(UDP ON)
endif (EXAMPLES_UDP)

add_subdirectory(echo)
