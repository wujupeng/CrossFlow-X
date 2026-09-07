add_library(cfx_s03_handoff STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    i_handoff_orchestrator.hpp
)

target_include_directories(cfx_s03_handoff PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(cfx_s03_handoff PUBLIC cfx_common)
