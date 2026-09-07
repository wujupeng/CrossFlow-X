add_library(cfx_platform_common STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    platform_ports.hpp
)

target_include_directories(cfx_platform_common PUBLIC
    ${CMAKE_SOURCE_DIR}/platform
)

target_link_libraries(cfx_platform_common PUBLIC cfx_common)
