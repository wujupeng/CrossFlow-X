add_library(cfx_s06_identity STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_node_identity_manager.hpp
    i_session_fence.hpp
    i_identity_recovery_manager.hpp
    node_identity_manager.hpp
    node_identity_manager.cpp
    session_fence.hpp
    session_fence.cpp
)

target_include_directories(cfx_s06_identity PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)