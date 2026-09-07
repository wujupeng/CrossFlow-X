add_library(cfx_s05_transport STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_control_plane_channel.hpp
    i_input_plane_channel.hpp
)

target_include_directories(cfx_s05_transport PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(cfx_s05_transport PUBLIC cfx_common)
