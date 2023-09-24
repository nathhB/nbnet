cmake_minimum_required(VERSION 3.1)
project(nbnet_examples)

add_subdirectory(echo)
add_subdirectory(echo_bytes)

if (NOT DEFINED CPP_COMPILE)
    # not supported in CPP
    add_subdirectory(rpc)
endif (CPP_COMPILE)
