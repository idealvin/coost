file(GLOB_RECURSE ALL_TEST_FILES "*.cc")

set(NON_STANDALONE_TESTS "http_serv.cc" "rpc.cc" "udp.cc" "ssl.cc")

foreach(TEST_FILE ${ALL_TEST_FILES})
    get_filename_component(TEST_FILE_NAME ${TEST_FILE} NAME)
    string(REPLACE ".cc" "" TEST_TARGET ${TEST_FILE_NAME})

    add_executable(${TEST_TARGET}_test ${TEST_FILE})

    set_target_properties(${TEST_TARGET}_test
        PROPERTIES OUTPUT_NAME ${TEST_TARGET}
    )

    target_link_libraries(${TEST_TARGET}_test PRIVATE co)

    list(FIND {NON_STANDALONE_TESTS TEST_FILE NOT_A_TEST)

    if (${TEST_FILE_NAME} IN_LIST NON_STANDALONE_TESTS)
      message("  - ${TEST_FILE} --> standalone test ${TEST_TARGET}")
    else()
      message("  - ${TEST_FILE} --> ${TEST_TARGET}")
      add_test(NAME ${TEST_TARGET}_test COMMAND ${TEST_TARGET}_test)
    endif()
endforeach(TEST_FILE ${ALL_TEST_FILES})
