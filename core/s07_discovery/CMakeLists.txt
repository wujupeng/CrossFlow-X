add_library(cfx_s07_discovery STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_discovery_service.hpp
    discovery_table.hpp
    discovery_table.cpp
    discovery_service.hpp
    discovery_service.cpp
    mdns.hpp
    mdns.cpp
)

target_include_directories(cfx_s07_discovery PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)
