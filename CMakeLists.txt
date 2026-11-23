cmake_minimum_required(VERSION 3.16)

project(embedded_firmware_update_lab
    VERSION 1.0.0
    DESCRIPTION "C++17 firmware update simulator with fault injection"
    LANGUAGES CXX)

option(FWUPDATE_BUILD_TESTS "Build the regression test suite" ON)
option(FWUPDATE_BUILD_DEMO "Build the demonstration executable" ON)
option(FWUPDATE_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang)" OFF)
option(FWUPDATE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---- Shared compile settings (applied per target, never globally) ----------
add_library(fwupdate_options INTERFACE)
target_compile_features(fwupdate_options INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(fwupdate_options INTERFACE /W4 /permissive-)
    if(FWUPDATE_WARNINGS_AS_ERRORS)
        target_compile_options(fwupdate_options INTERFACE /WX)
    endif()
else()
    target_compile_options(fwupdate_options INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
    if(FWUPDATE_WARNINGS_AS_ERRORS)
        target_compile_options(fwupdate_options INTERFACE -Werror)
    endif()
endif()

if(FWUPDATE_ENABLE_SANITIZERS)
    if(MSVC)
        message(WARNING "FWUPDATE_ENABLE_SANITIZERS: ASan/UBSan combination is only wired up for GCC/Clang; ignored for MSVC")
    else()
        set(_fw_san -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
        target_compile_options(fwupdate_options INTERFACE ${_fw_san})
        target_link_options(fwupdate_options INTERFACE ${_fw_san})
        message(STATUS "fwupdate: AddressSanitizer + UndefinedBehaviorSanitizer enabled")
    endif()
endif()

# ---- Core library ----------------------------------------------------------
add_library(fwupdate_core
    src/crc32.cpp
    src/protocol.cpp
    src/packet.cpp
    src/receiver.cpp
    src/sender.cpp)
target_include_directories(fwupdate_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fwupdate_core PUBLIC fwupdate_options)

# ---- Fault injection (test / demo support, kept out of the core) -----------
add_library(fwupdate_faults src/fault_injection.cpp)
target_include_directories(fwupdate_faults PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fwupdate_faults PUBLIC fwupdate_core)

# ---- Demo ------------------------------------------------------------------
if(FWUPDATE_BUILD_DEMO)
    add_executable(fwupdate_demo examples/demo.cpp)
    target_link_libraries(fwupdate_demo PRIVATE fwupdate_faults)
endif()

# ---- Tests -----------------------------------------------------------------
if(FWUPDATE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    if(FWUPDATE_BUILD_DEMO)
        add_test(NAME DEMO_end_to_end COMMAND fwupdate_demo)
    endif()
endif()
