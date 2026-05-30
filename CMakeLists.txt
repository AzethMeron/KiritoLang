cmake_minimum_required(VERSION 3.28)
project(Kirito CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The interpreter is header-only: an INTERFACE target that just exposes include/.
add_library(kirito INTERFACE)
target_include_directories(kirito INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(kirito INTERFACE cxx_std_20)

# The only compiled translation unit with a main().
add_executable(ki main.cpp)
target_link_libraries(ki PRIVATE kirito)
target_compile_options(ki PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_subdirectory(tests)
