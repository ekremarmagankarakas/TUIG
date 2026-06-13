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

add_subdirectory(chesscli)
add_subdirectory(damacli)

include(FetchContent)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ftxui
  URL      https://github.com/ArthurSonzogni/ftxui/archive/refs/tags/v5.0.0.tar.gz
  URL_HASH SHA256=a2991cb222c944aee14397965d9f6b050245da849d8c5da7c72d112de2786b5b
)
FetchContent_MakeAvailable(ftxui)

add_executable(tuig src/main.cc)
target_link_libraries(tuig PRIVATE ftxui::component)
