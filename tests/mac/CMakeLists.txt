add_executable(test_mac_platform
    test_mac_platform.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_field_extractor.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/cg_event_normalizer.cpp
)
target_link_libraries(test_mac_platform PRIVATE cfx_platform_common)
target_include_directories(test_mac_platform PRIVATE ${CMAKE_SOURCE_DIR}/platform)
add_test(NAME test_mac_platform COMMAND test_mac_platform)
