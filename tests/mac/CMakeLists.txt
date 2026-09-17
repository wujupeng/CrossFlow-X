add_executable(test_mac_platform
    test_mac_platform.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_tap.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/a11y_permission_guard.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_field_extractor.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/cg_event_normalizer.cpp
)
target_link_libraries(test_mac_platform PRIVATE cfx_platform_common)
target_include_directories(test_mac_platform PRIVATE ${CMAKE_SOURCE_DIR}/platform)

if(CFX_PLATFORM_MAC)
    find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(APPLICATION_SERVICES ApplicationServices REQUIRED)
    find_library(CARBON Carbon REQUIRED)
    target_link_libraries(test_mac_platform PRIVATE
        ${CORE_GRAPHICS}
        ${CORE_FOUNDATION}
        ${APPLICATION_SERVICES}
        ${CARBON}
    )
endif()

add_test(NAME test_mac_platform COMMAND test_mac_platform)

add_executable(test_dual_channel
    test_dual_channel.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/dual_channel_spsc.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/authoritative_resync.cpp
)
target_link_libraries(test_dual_channel PRIVATE cfx_platform_common)
target_include_directories(test_dual_channel PRIVATE ${CMAKE_SOURCE_DIR}/platform)

if(CFX_PLATFORM_MAC)
    find_library(DC_CORE_GRAPHICS CoreGraphics REQUIRED)
    find_library(DC_CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(DC_APPLICATION_SERVICES ApplicationServices REQUIRED)
    target_link_libraries(test_dual_channel PRIVATE
        ${DC_CORE_GRAPHICS}
        ${DC_CORE_FOUNDATION}
        ${DC_APPLICATION_SERVICES}
    )
endif()

add_test(NAME test_dual_channel COMMAND test_dual_channel)

