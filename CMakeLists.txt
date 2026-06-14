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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -g")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

# Skip subproject tests by default.
set(CHESSCLI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAMACLI_BUILD_TESTS  OFF CACHE BOOL "" FORCE)

# Vendored ftxui (see vendor/ftxui submodule, pinned to v5.0.0). Added before
# the games so their CMakeLists' `if(NOT TARGET ftxui::component)` guard sees
# the targets already defined and skips its own FetchContent fallback.
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/ftxui EXCLUDE_FROM_ALL)

add_subdirectory(chesscli)
add_subdirectory(damacli)

add_executable(tuig src/main.cc)
target_link_libraries(tuig PRIVATE ftxui::component)
