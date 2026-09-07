add_library(cfx_s01_event STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_event_normalizer.hpp
)

target_include_directories(cfx_s01_event PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(cfx_s01_event PUBLIC cfx_common)
