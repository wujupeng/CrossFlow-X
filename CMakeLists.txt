cmake_minimum_required(VERSION 3.20)

project(CrossFlow-X
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "CrossFlow-X: Driverless user-mode cross-platform input flow forwarding"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Platform)

if(CFX_PLATFORM_MAC)
    add_compile_definitions(CFX_PLATFORM_MAC=1)
elseif(CFX_PLATFORM_WIN)
    add_compile_definitions(CFX_PLATFORM_WIN=1)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Werror -Wpedantic)
endif()

enable_testing()

add_subdirectory(core/common)
add_subdirectory(core/s01_event)
add_subdirectory(core/s02_topology)
add_subdirectory(core/s03_handoff)
add_subdirectory(core/s04_coord)
add_subdirectory(core/s05_transport)
add_subdirectory(platform/common)
add_subdirectory(platform/mac)
add_subdirectory(platform/win)

add_executable(cfx_agent src/main.cpp)
target_link_libraries(cfx_agent PRIVATE
    cfx_common
    cfx_s01_event
    cfx_s02_topology
    cfx_s03_handoff
    cfx_s04_coord
    cfx_s05_transport
    cfx_platform_common
)
if(CFX_PLATFORM_MAC)
    target_link_libraries(cfx_agent PRIVATE cfx_platform_mac)
elseif(CFX_PLATFORM_WIN)
    target_link_libraries(cfx_agent PRIVATE cfx_platform_win)
endif()

add_subdirectory(tests)