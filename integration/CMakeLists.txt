add_library(cfx_integration STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    e2e_pipeline_orchestrator.hpp
    e2e_pipeline_orchestrator.cpp
    disconnect_resync_coordinator.hpp
    disconnect_resync_coordinator.cpp
    e2e_latency_probe.hpp
    e2e_latency_probe.cpp
    e2e_evidence_collector.hpp
    e2e_evidence_collector.cpp
    final_evidence_matrix_generator.hpp
    final_evidence_matrix_generator.cpp
    frozen_red_line_checker.hpp
    frozen_red_line_checker.cpp
)

target_include_directories(cfx_integration PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/platform
)

target_link_libraries(cfx_integration PUBLIC
    cfx_common
    cfx_s05_transport
    cfx_platform_common
)