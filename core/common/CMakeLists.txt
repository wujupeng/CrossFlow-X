add_library(cfx_common STATIC
    ${CMAKE_SOURCE_DIR}/cmake/placeholder.cpp
    version.hpp
    error_code.hpp
    error_code.cpp
    logger.hpp
    logger.cpp
    domain.hpp
    domain.cpp
    messages.hpp
)

target_include_directories(cfx_common PUBLIC
    ${CMAKE_SOURCE_DIR}/core
)
