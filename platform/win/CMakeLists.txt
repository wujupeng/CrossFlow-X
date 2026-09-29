if(NOT CFX_PLATFORM_WIN)
    return()
endif()

add_library(cfx_platform_win STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    win_event_injector.hpp
    win_event_injector.cpp
    win_input_plane_receiver.hpp
    win_input_plane_receiver.cpp
)

target_include_directories(cfx_platform_win PUBLIC
    ${CMAKE_SOURCE_DIR}/platform
)

target_link_libraries(cfx_platform_win PUBLIC cfx_platform_common)
