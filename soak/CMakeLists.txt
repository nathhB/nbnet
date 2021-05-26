cmake_minimum_required(VERSION 3.0)

project(soak C)

add_executable(client client.c soak.c logging.c)
add_executable(server server.c soak.c logging.c)

target_link_libraries(client pthread m)
target_link_libraries(server pthread m)

add_compile_options(-Wall -Wextra)

target_compile_definitions(client PUBLIC NBN_DEBUG NBN_DISABLE_STALE_CONNECTION_DETECTION NBN_USE_PACKET_SIMULATOR SOAK_CLIENT)
target_compile_definitions(server PUBLIC NBN_DEBUG NBN_DISABLE_STALE_CONNECTION_DETECTION NBN_USE_PACKET_SIMULATOR SOAK_SERVER)

option(ENCRYPTION_ENABLED OFF)

if (ENCRYPTION_ENABLED)
  message("Encryption enabled")

  target_compile_definitions(client PUBLIC SOAK_ENCRYPTION)
  target_compile_definitions(server PUBLIC SOAK_ENCRYPTION)
endif(ENCRYPTION_ENABLED)

unset(ENCRYPTION_ENABLED)

if(WIN32)
    target_link_libraries(client wsock32 ws2_32)
    target_link_libraries(server wsock32 ws2_32)
endif()

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_MEMORY=30MB -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --js-library ${CMAKE_CURRENT_SOURCE_DIR}/../net_drivers/webrtc/js/api.js")
endif()
