cmake_minimum_required(VERSION 3.16)
project(aquila LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(aquila src/main.cpp)
target_compile_options(aquila PRIVATE -Wall -Wextra -Wpedantic)

if(NOT MSVC)
  target_compile_options(aquila PRIVATE -O3)
endif()

enable_testing()
add_test(NAME perft COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/perft_suite.sh)
