add_executable(test_mac_platform
    test_mac_platform.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_tap.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/a11y_permission_guard.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_field_extractor.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/cg_event_normalizer.cpp
)
target_link_libraries(test_mac_platform PRIVATE cfx_platform_common)
target_include_directories(test_mac_platform PRIVATE ${CMAKE_SOURCE_DIR}/platform)
add_test(NAME test_mac_platform COMMAND test_mac_platform)

add_executable(test_group3
    test_group3.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/dual_channel_spsc.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/authoritative_resync.cpp
)
target_link_libraries(test_group3 PRIVATE cfx_platform_common)
target_include_directories(test_group3 PRIVATE ${CMAKE_SOURCE_DIR}/platform)
add_test(NAME test_group3 COMMAND test_group3)
