cmake_minimum_required(VERSION 3.6)
project(test)

set(SOURCES server.c)
add_executable(server ${SOURCES})
target_link_libraries(server "/Users/nathb/Dev/GameDev/nbnet/lib/udp/build/libnbnet_udp.dylib")
