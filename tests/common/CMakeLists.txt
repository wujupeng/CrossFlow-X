add_executable(test_version test_version.cpp)
target_link_libraries(test_version PRIVATE cfx_common)
add_test(NAME test_version COMMAND test_version)

add_executable(test_error_code test_error_code.cpp)
target_link_libraries(test_error_code PRIVATE cfx_common)
add_test(NAME test_error_code COMMAND test_error_code)

add_executable(test_logger test_logger.cpp)
target_link_libraries(test_logger PRIVATE cfx_common)
add_test(NAME test_logger COMMAND test_logger)

add_executable(test_domain test_domain.cpp)
target_link_libraries(test_domain PRIVATE cfx_common)
add_test(NAME test_domain COMMAND test_domain)

add_executable(test_platform_ports test_platform_ports.cpp)
target_link_libraries(test_platform_ports PRIVATE cfx_platform_common)
add_test(NAME test_platform_ports COMMAND test_platform_ports)

add_executable(test_atomic_payload test_atomic_payload.cpp)
target_link_libraries(test_atomic_payload PRIVATE cfx_platform_common)
add_test(NAME test_atomic_payload COMMAND test_atomic_payload)

add_executable(test_atomic_payload_platform test_atomic_payload_platform.cpp)
target_link_libraries(test_atomic_payload_platform PRIVATE cfx_platform_common)
add_test(NAME test_atomic_payload_platform COMMAND test_atomic_payload_platform)

add_executable(test_spsc_ring_buffer test_spsc_ring_buffer.cpp)
target_link_libraries(test_spsc_ring_buffer PRIVATE cfx_platform_common)
add_test(NAME test_spsc_ring_buffer COMMAND test_spsc_ring_buffer)

add_executable(test_spsc_invariants test_spsc_invariants.cpp)
target_link_libraries(test_spsc_invariants PRIVATE cfx_platform_common)
add_test(NAME test_spsc_invariants COMMAND test_spsc_invariants)

add_executable(test_r18_linearization test_r18_linearization.cpp)
target_link_libraries(test_r18_linearization PRIVATE cfx_platform_common)
add_test(NAME test_r18_linearization COMMAND test_r18_linearization)

add_executable(test_spsc_slot test_spsc_slot.cpp)
target_link_libraries(test_spsc_slot PRIVATE cfx_platform_common)
add_test(NAME test_spsc_slot COMMAND test_spsc_slot)

add_executable(test_business_interfaces test_business_interfaces.cpp)
target_link_libraries(test_business_interfaces PRIVATE
    cfx_common
    cfx_s01_event
    cfx_s02_topology
    cfx_s03_handoff
    cfx_s04_coord
    cfx_s05_transport
    cfx_platform_common
)
add_test(NAME test_business_interfaces COMMAND test_business_interfaces)
