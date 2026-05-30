cmake_minimum_required(VERSION 3.28)
project(Kirito CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional HTTPS support in the `net` module (links OpenSSL). Off by default so the core stays
# dependency-free; enable with -DKIRITO_ENABLE_TLS=ON.
option(KIRITO_ENABLE_TLS "Enable HTTPS in the net module via OpenSSL" OFF)

# Static linking for all builds, so the binaries don't depend on a matching libstdc++/libgcc on
# the target machine. We statically link the C++ runtime and libgcc; on MSVC we use the static
# CRT. We deliberately do NOT force a fully static libc (-static) on Linux: glibc's name-service
# (getaddrinfo) needs shared NSS modules at runtime, and a full static link only warns about that
# while offering no portability gain over a static C++ runtime.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_link_options(-static-libgcc -static-libstdc++)
    if(MINGW)
        # On Windows/MinGW a full static link is clean and desirable (no NSS concern).
        add_link_options(-static)
    endif()
endif()

# The interpreter is header-only: an INTERFACE target that just exposes include/. It also carries
# the platform's socket library so any consumer (CLI or tests) links it transparently.
add_library(kirito INTERFACE)
target_include_directories(kirito INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(kirito INTERFACE cxx_std_20)
if(WIN32)
    target_link_libraries(kirito INTERFACE ws2_32)  # Winsock
endif()

# The only compiled translation unit with a main().
add_executable(ki main.cpp)
target_link_libraries(ki PRIVATE kirito)
if(NOT MSVC)
    target_compile_options(ki PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(KIRITO_ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
    target_compile_definitions(ki PRIVATE KIRITO_ENABLE_TLS)
    target_link_libraries(ki PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

enable_testing()
add_subdirectory(tests)
