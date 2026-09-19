cmake_minimum_required(VERSION 3.16)
project(aquila LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(aquila src/main.cpp)
target_compile_options(aquila PRIVATE -Wall -Wextra -Wpedantic)

if(NOT MSVC)
  target_compile_options(aquila PRIVATE -O3)
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(aquila_core_tests tests/core_regression.cpp)
target_compile_options(aquila_core_tests PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME perft COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/perft_suite.sh)
add_test(NAME core_regression COMMAND aquila_core_tests)
add_test(
  NAME uci_regression
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/uci_regression.py $<TARGET_FILE:aquila>
)
