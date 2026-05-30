cmake_minimum_required(VERSION 3.28)
project(Kirito CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional HTTPS support in the `net` module (links OpenSSL). Off by default so the core stays
# dependency-free; enable with -DKIRITO_ENABLE_TLS=ON.
option(KIRITO_ENABLE_TLS "Enable HTTPS in the net module via OpenSSL" OFF)

# The interpreter is header-only: an INTERFACE target that just exposes include/.
add_library(kirito INTERFACE)
target_include_directories(kirito INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(kirito INTERFACE cxx_std_20)

# The only compiled translation unit with a main().
add_executable(ki main.cpp)
target_link_libraries(ki PRIVATE kirito)
target_compile_options(ki PRIVATE -Wall -Wextra -Wpedantic)

if(KIRITO_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(ki PRIVATE KIRITO_ENABLE_TLS)
    target_link_libraries(ki PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

enable_testing()
add_subdirectory(tests)
