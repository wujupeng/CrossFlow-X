add_library(cfx_s04_coord STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_coord_mapper.hpp
)

target_include_directories(cfx_s04_coord PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(cfx_s04_coord PUBLIC cfx_common)
