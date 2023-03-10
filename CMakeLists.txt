cmake_minimum_required(VERSION 3.20)
project(rag C)

# general configuration
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# enable IPO on Release (if supported)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if (ipo_supported)
    set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif ()

add_subdirectory(src)
add_subdirectory(deps)
add_subdirectory(test)
