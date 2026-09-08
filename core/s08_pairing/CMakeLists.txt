add_library(cfx_s08_pairing STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_pairing_manager.hpp
    i_registration_manager.hpp
    i_trusted_node_list.hpp
    pairing_fsm.hpp
    pairing_fsm.cpp
    trusted_node_list.hpp
    trusted_node_list.cpp
)

target_include_directories(cfx_s08_pairing PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)