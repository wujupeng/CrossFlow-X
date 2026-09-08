add_library(cfx_s07_discovery STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_discovery_service.hpp
)

target_include_directories(cfx_s07_discovery PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)