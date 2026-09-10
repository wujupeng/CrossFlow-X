add_executable(test_cf1 test_cf1.cpp)
target_link_libraries(test_cf1 PRIVATE
    cfx_common
    cfx_s06_identity
    cfx_s07_discovery
    cfx_s08_pairing
    cfx_s09_membership
)
add_test(NAME test_cf1 COMMAND test_cf1)

add_executable(test_cf1_integration test_cf1_integration.cpp)
target_link_libraries(test_cf1_integration PRIVATE
    cfx_common
    cfx_s06_identity
    cfx_s07_discovery
    cfx_s08_pairing
    cfx_s09_membership
)
add_test(NAME test_cf1_integration COMMAND test_cf1_integration)

add_executable(test_cf1_contracts test_cf1_contracts.cpp)
target_link_libraries(test_cf1_contracts PRIVATE
    cfx_common
    cfx_s06_identity
    cfx_s07_discovery
    cfx_s08_pairing
    cfx_s09_membership
)
add_test(NAME test_cf1_contracts COMMAND test_cf1_contracts)

add_executable(mdns_announcer_tool physical_mdns_interop/mdns_announcer_tool.cpp)
target_link_libraries(mdns_announcer_tool PRIVATE
    cfx_common
    cfx_s07_discovery
)

add_executable(mdns_listener_tool physical_mdns_interop/mdns_listener_tool.cpp)
target_link_libraries(mdns_listener_tool PRIVATE
    cfx_common
    cfx_s07_discovery
)