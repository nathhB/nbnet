cmake_minimum_required(VERSION 3.0)

project(ping_pong)

option(CPP_COMPILE OFF)

# allow to compile as cpp
if (CPP_COMPILE)
  file(GLOB_RECURSE CFILES "${CMAKE_SOURCE_DIR}/*.c")
  SET_SOURCE_FILES_PROPERTIES(${CFILES} PROPERTIES LANGUAGE CXX)
endif (CPP_COMPILE)

unset(CPP_COMPILE)

add_executable(ping_pong ping_pong.c logging.c)

add_compile_options(-Wall -Wextra -Wpedantic)

target_compile_definitions(ping_pong PUBLIC NBN_DEBUG NBN_ROOMS NBN_USE_PACKET_SIMULATOR)

option(ENCRYPTION_ENABLED OFF)

if(WIN32)
  target_link_libraries(ping_pong wsock32 ws2_32)
else()
  # link with pthread when we are not on windows
  target_link_libraries(ping_pong pthread)
endif(WIN32)

if (UNIX)
  # link with libm on unix
  target_link_libraries(ping_pong m)
endif (UNIX)

if (EMSCRIPTEN)
  set(NBNET_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../")
  set(ASYNCIFY_IMPORTS "[\"__js_game_server_start\", \"__js_game_client_start\", \"__js_game_client_close\"]")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1 -s TOTAL_MEMORY=30MB -s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=4 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -s ASYNCIFY -s ASYNCIFY_IMPORTS=\"${ASYNCIFY_IMPORTS}\"")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --js-library ${NBNET_ROOT_DIR}/net_drivers/webrtc/js/api_common.js --js-library ${NBNET_ROOT_DIR}/net_drivers/webrtc/js/api_room.js")
endif()
