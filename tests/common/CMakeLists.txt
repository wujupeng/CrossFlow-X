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
