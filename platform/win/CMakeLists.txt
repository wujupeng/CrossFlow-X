if(NOT CFX_PLATFORM_WIN)
    return()
endif()

add_library(cfx_platform_win STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
)

target_include_directories(cfx_platform_win PUBLIC
    ${CMAKE_SOURCE_DIR}/platform
)

target_link_libraries(cfx_platform_win PUBLIC cfx_platform_common)
