add_executable(test_e2e_pipeline
    test_e2e_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/core/s05_transport/transport_impl.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_pipeline_orchestrator.cpp
)
target_link_libraries(test_e2e_pipeline PRIVATE
    cfx_common
    cfx_s05_transport
    cfx_platform_common
    cfx_integration
)
target_include_directories(test_e2e_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/platform
)

if(CFX_PLATFORM_WIN)
    target_sources(test_e2e_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}/platform/win/win_event_injector.cpp
        ${CMAKE_SOURCE_DIR}/platform/win/win_input_plane_receiver.cpp
    )
endif()

add_test(NAME test_e2e_pipeline COMMAND test_e2e_pipeline)
add_executable(test_integration_support
    test_integration_support.cpp
    ${CMAKE_SOURCE_DIR}/core/s05_transport/transport_impl.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_pipeline_orchestrator.cpp
    ${CMAKE_SOURCE_DIR}/integration/disconnect_resync_coordinator.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_latency_probe.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_evidence_collector.cpp
)
target_link_libraries(test_integration_support PRIVATE
    cfx_common
    cfx_s05_transport
    cfx_platform_common
    cfx_integration
)
target_include_directories(test_integration_support PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/platform
)

add_test(NAME test_integration_support COMMAND test_integration_support)
set(E2E_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/core/s05_transport/transport_impl.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_pipeline_orchestrator.cpp
    ${CMAKE_SOURCE_DIR}/integration/disconnect_resync_coordinator.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_latency_probe.cpp
    ${CMAKE_SOURCE_DIR}/integration/e2e_evidence_collector.cpp
)

set(E2E_TEST_LIBS cfx_common cfx_s05_transport cfx_platform_common cfx_integration)
set(E2E_TEST_INCLUDES ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core ${CMAKE_SOURCE_DIR}/platform)

function(add_e2e_test name source)
    add_executable(${name} ${source} ${E2E_TEST_SOURCES})
    target_link_libraries(${name} PRIVATE ${E2E_TEST_LIBS})
    target_include_directories(${name} PRIVATE ${E2E_TEST_INCLUDES})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_e2e_test(test_e2e_01_mouse_move test_e2e_01_mouse_move.cpp)
add_e2e_test(test_e2e_02_mouse_boundary test_e2e_02_mouse_boundary.cpp)
add_e2e_test(test_e2e_03_mouse_button test_e2e_03_mouse_button.cpp)
add_e2e_test(test_e2e_04_modifier test_e2e_04_modifier.cpp)
add_e2e_test(test_e2e_05_disconnect_resync test_e2e_05_disconnect_resync.cpp)
add_e2e_test(test_e2e_06_release_all_pressed test_e2e_06_release_all_pressed.cpp)
add_e2e_test(test_final_evidence test_final_evidence.cpp)