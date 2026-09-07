add_library(cfx_s02_topology STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_topology_manager.hpp
)

target_include_directories(cfx_s02_topology PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(cfx_s02_topology PUBLIC cfx_common)
