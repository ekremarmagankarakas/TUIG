cmake_minimum_required(VERSION 3.20)
project(tuig CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Skip subproject tests by default.
set(CHESSCLI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAMACLI_BUILD_TESTS  OFF CACHE BOOL "" FORCE)

add_subdirectory(chesscli)
add_subdirectory(damacli)

add_executable(tuig src/main.cc)
target_link_libraries(tuig PRIVATE)

# Pass game binary paths to tuig at compile time via generator expressions.
target_compile_definitions(tuig PRIVATE
  TUIG_CHESS_BIN="$<TARGET_FILE:chesscli>"
  TUIG_DAMA_BIN="$<TARGET_FILE:damacli>"
)

add_dependencies(tuig chesscli damacli)
