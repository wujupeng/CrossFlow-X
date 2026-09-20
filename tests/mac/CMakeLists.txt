add_executable(test_mac_platform
    test_mac_platform.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_tap.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/a11y_permission_guard.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_field_extractor.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/cg_event_normalizer.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/dual_channel_spsc.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/authoritative_resync.cpp
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

add_executable(test_callback_boundary
    test_callback_boundary.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/dual_channel_spsc.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/authoritative_resync.cpp
)
target_link_libraries(test_callback_boundary PRIVATE cfx_platform_common)
target_include_directories(test_callback_boundary PRIVATE ${CMAKE_SOURCE_DIR}/platform)

if(CFX_PLATFORM_MAC)
    target_link_libraries(test_callback_boundary PRIVATE
        ${DC_CORE_GRAPHICS}
        ${DC_CORE_FOUNDATION}
        ${DC_APPLICATION_SERVICES}
    )
endif()

add_test(NAME test_callback_boundary COMMAND test_callback_boundary)

add_executable(test_saturation_recovery
    test_saturation_recovery.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/dual_channel_spsc.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/authoritative_resync.cpp
)
target_link_libraries(test_saturation_recovery PRIVATE cfx_platform_common)
target_include_directories(test_saturation_recovery PRIVATE ${CMAKE_SOURCE_DIR}/platform)

if(CFX_PLATFORM_MAC)
    target_link_libraries(test_saturation_recovery PRIVATE
        ${DC_CORE_GRAPHICS}
        ${DC_CORE_FOUNDATION}
        ${DC_APPLICATION_SERVICES}
    )
endif()

add_test(NAME test_saturation_recovery COMMAND test_saturation_recovery)

add_executable(test_mac_injector
    test_mac_injector.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_injector.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/release_all_pressed_executor.cpp
)
target_link_libraries(test_mac_injector PRIVATE cfx_platform_common)
target_include_directories(test_mac_injector PRIVATE ${CMAKE_SOURCE_DIR}/platform)

if(CFX_PLATFORM_MAC)
    find_library(INJ_CORE_GRAPHICS CoreGraphics REQUIRED)
    find_library(INJ_CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(INJ_APPLICATION_SERVICES ApplicationServices REQUIRED)
    find_library(INJ_CARBON Carbon REQUIRED)
    target_link_libraries(test_mac_injector PRIVATE
        ${INJ_CORE_GRAPHICS}
        ${INJ_CORE_FOUNDATION}
        ${INJ_APPLICATION_SERVICES}
        ${INJ_CARBON}
    )
endif()

add_test(NAME test_mac_injector COMMAND test_mac_injector)

add_executable(physical_evidence_phase_e
    physical_evidence_phase_e.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/mac_event_injector.cpp
    ${CMAKE_SOURCE_DIR}/platform/mac/release_all_pressed_executor.cpp
)
target_link_libraries(physical_evidence_phase_e PRIVATE cfx_platform_common)
target_include_directories(physical_evidence_phase_e PRIVATE ${CMAKE_SOURCE_DIR}/platform)

if(CFX_PLATFORM_MAC)
    find_library(PE_CORE_GRAPHICS CoreGraphics REQUIRED)
    find_library(PE_CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(PE_APPLICATION_SERVICES ApplicationServices REQUIRED)
    find_library(PE_CARBON Carbon REQUIRED)
    target_link_libraries(physical_evidence_phase_e PRIVATE
        ${PE_CORE_GRAPHICS}
        ${PE_CORE_FOUNDATION}
        ${PE_APPLICATION_SERVICES}
        ${PE_CARBON}
    )
endif()

