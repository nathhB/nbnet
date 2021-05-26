cmake_minimum_required(VERSION 3.0)

project(raylib_example C)

set(raylib_DIR cmake)

set(CLIENT_SOURCES client.c shared.c)
set(SERVER_SOURCES server.c shared.c)

add_compile_options(-Wall -Wextra -Wno-unknown-pragmas -Wno-type-limits)

add_executable(client ${CLIENT_SOURCES})
add_executable(server ${SERVER_SOURCES})

# set RAYLIB_LIBRARY_PATH from command line

target_link_libraries(client ${RAYLIB_LIBRARY_PATH} pthread m)
target_link_libraries(server ${RAYLIB_LIBRARY_PATH} pthread m)

# set RAYLIB_INCLUDE_PATH from command line

target_include_directories(client PUBLIC "${RAYLIB_INCLUDE_PATH}")
target_include_directories(server PUBLIC "${RAYLIB_INCLUDE_PATH}")

if(WIN32)
    target_link_libraries(client wsock32 ws2_32 opengl32 gdi32 winmm)
    target_link_libraries(server wsock32 ws2_32)
endif()

target_compile_definitions(client PUBLIC NBN_DEBUG NBN_USE_PACKET_SIMULATOR)
target_compile_definitions(server PUBLIC NBN_DEBUG NBN_USE_PACKET_SIMULATOR)

if (ENCRYPTION_ENABLED)
  message("Encryption enabled")

  target_compile_definitions(client PUBLIC EXAMPLE_ENCRYPTION)
  target_compile_definitions(server PUBLIC EXAMPLE_ENCRYPTION)
endif(ENCRYPTION_ENABLED)

if (EMSCRIPTEN)
    set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_PTHREADS=1 -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_MEMORY=30MB -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --js-library ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/api.js")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
    set_target_properties(client PROPERTIES SUFFIX ".html")

    add_custom_command(
        TARGET client
        POST_BUILD
        COMMAND browserify ${CMAKE_CURRENT_SOURCE_DIR}/../../net_drivers/webrtc/js/nbnet.js -o nbnet_bundle.js)
endif()

if (APPLE)
  target_link_libraries(client "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo")
endif (APPLE)
