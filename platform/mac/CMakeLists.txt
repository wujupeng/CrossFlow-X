if(NOT CFX_PLATFORM_MAC)
    return()
endif()

add_library(cfx_platform_mac STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
)

target_include_directories(cfx_platform_mac PUBLIC
    ${CMAKE_SOURCE_DIR}/platform
)

target_link_libraries(cfx_platform_mac PUBLIC cfx_platform_common)

find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
target_link_libraries(cfx_platform_mac PRIVATE ${CORE_GRAPHICS} ${CORE_FOUNDATION})
