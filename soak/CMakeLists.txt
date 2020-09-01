cmake_minimum_required(VERSION 3.0)

project(soak C)

add_executable(client client.c soak.c logging.c)
add_executable(server server.c soak.c logging.c)

# for packet simulator
target_link_libraries(client pthread)
target_link_libraries(server pthread)

add_compile_options(-Wall -Wextra)

target_compile_definitions(client PUBLIC
    NBN_GAME_CLIENT NBN_DEBUG NBN_DISABLE_STALE_CONNECTION_DETECTION NBN_USE_PACKET_SIMULATOR)
target_compile_definitions(server PUBLIC
    NBN_GAME_SERVER NBN_DEBUG NBN_DISABLE_STALE_CONNECTION_DETECTION NBN_USE_PACKET_SIMULATOR)

if(WIN32)
    target_link_libraries(client wsock32 ws2_32)
    target_link_libraries(server wsock32 ws2_32)
endif()