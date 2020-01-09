include_directories(${CMAKE_SOURCE_DIR})

list(APPEND LIBS_TO_LINK base)
if(UNIX)
  list(APPEND LIBS_TO_LINK pthread dl)
endif()

file(GLOB ALL_TEST_FILES "*_test.cc")
foreach(TEST_FILE ${ALL_TEST_FILES})
  get_filename_component(TEST_FILE ${TEST_FILE} NAME)
  string(REPLACE "_test.cc" "" TEST_TARGET ${TEST_FILE})
  message("  - ${TEST_FILE} --> ${TEST_TARGET}")
  add_executable(${TEST_TARGET} ${TEST_FILE})
  target_link_libraries(${TEST_TARGET}
    ${LIBS_TO_LINK}
  )
endforeach(TEST_FILE ${ALL_TEST_FILES})
