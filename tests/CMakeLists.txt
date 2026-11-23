set(FWUPDATE_TEST_SOURCES
    test_crc32.cpp
    test_packet.cpp
    test_receiver.cpp
    test_faults.cpp
    test_robustness.cpp)

add_executable(fwupdate_tests testkit_main.cpp ${FWUPDATE_TEST_SOURCES})
target_link_libraries(fwupdate_tests PRIVATE fwupdate_faults)

# Register every FW_TEST(...) as an individual CTest test so each scenario is
# reported (and can be re-run) on its own.
set(_fw_test_count 0)
foreach(_src IN LISTS FWUPDATE_TEST_SOURCES)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${_src}" _lines REGEX "^FW_TEST\\(")
    foreach(_line IN LISTS _lines)
        string(REGEX REPLACE "^FW_TEST\\(([A-Za-z0-9_]+),.*$" "\\1" _name "${_line}")
        add_test(NAME ${_name} COMMAND fwupdate_tests ${_name})
        math(EXPR _fw_test_count "${_fw_test_count} + 1")
    endforeach()
endforeach()
message(STATUS "fwupdate: registered ${_fw_test_count} test cases with CTest")
