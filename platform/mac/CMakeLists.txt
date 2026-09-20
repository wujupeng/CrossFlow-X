if(NOT CFX_PLATFORM_MAC)
    return()
endif()

add_library(cfx_platform_mac STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    mac_event_tap.hpp
    mac_event_tap.cpp
    a11y_permission_guard.hpp
    a11y_permission_guard.cpp
    mac_event_field_extractor.hpp
    mac_event_field_extractor.cpp
    capture_handle.hpp
    cg_event_normalizer.hpp
    cg_event_normalizer.cpp
    dual_channel_spsc.hpp
    dual_channel_spsc.cpp
    authoritative_resync.hpp
    authoritative_resync.cpp
    native_coord_normalizer.hpp
    native_coord_normalizer.cpp
    screen_boundary_cache.hpp
    screen_boundary_cache.cpp
    multi_display_merge_policy.hpp
    multi_display_merge_policy.cpp
    mac_screen_query.hpp
    mac_screen_query.cpp
)

target_include_directories(cfx_platform_mac PUBLIC
    ${CMAKE_SOURCE_DIR}/platform
)

target_link_libraries(cfx_platform_mac PUBLIC cfx_platform_common)

find_library(CORE_GRAPHICS CoreGraphics REQUIRED)
find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
find_library(APPLICATION_SERVICES ApplicationServices REQUIRED)
find_library(CARBON Carbon REQUIRED)
target_link_libraries(cfx_platform_mac PRIVATE
    ${CORE_GRAPHICS}
    ${CORE_FOUNDATION}
    ${APPLICATION_SERVICES}
    ${CARBON}
)
