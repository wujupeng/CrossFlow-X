add_library(cfx_s07_discovery STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_discovery_service.hpp
    discovery_table.hpp
    discovery_table.cpp
    discovery_service.hpp
    discovery_service.cpp
    udp_broadcast.hpp
    udp_broadcast.cpp
    mdns.hpp
    mdns.cpp
)

target_include_directories(cfx_s07_discovery PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)

if(CFX_PLATFORM_WIN)
    target_link_libraries(cfx_s07_discovery PUBLIC ws2_32)
endif()
