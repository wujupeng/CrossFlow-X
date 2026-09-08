add_executable(test_cf1 test_cf1.cpp)
target_link_libraries(test_cf1 PRIVATE
    cfx_common
    cfx_s06_identity
    cfx_s07_discovery
    cfx_s08_pairing
    cfx_s09_membership
)
add_test(NAME test_cf1 COMMAND test_cf1)