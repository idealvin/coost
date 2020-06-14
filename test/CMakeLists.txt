list(APPEND LIBS_TO_LINK co)
if(UNIX)
    list(APPEND LIBS_TO_LINK pthread dl)
endif()

file(GLOB_RECURSE ALL_TEST_FILES "*_test.cc")

foreach(TEST_FILE ${ALL_TEST_FILES})
    get_filename_component(TEST_FILE_NAME ${TEST_FILE} NAME)
    string(REPLACE "_test.cc" "" TEST_TARGET ${TEST_FILE_NAME})
    message("  - ${TEST_FILE} --> ${TEST_TARGET}")

    add_executable(${TEST_TARGET}_test ${TEST_FILE})

    set_target_properties(${TEST_TARGET}_test
        PROPERTIES OUTPUT_NAME ${TEST_TARGET}
    )

    target_link_libraries(${TEST_TARGET}_test
        ${LIBS_TO_LINK}
    )
endforeach(TEST_FILE ${ALL_TEST_FILES})
