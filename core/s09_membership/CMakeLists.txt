add_library(cfx_s09_membership STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_membership_manager.hpp
    i_topology_version_manager.hpp
    membership_manager.hpp
    membership_manager.cpp
    topology_version_manager.hpp
    topology_version_manager.cpp
    topology_change_event_source.hpp
)

target_include_directories(cfx_s09_membership PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)