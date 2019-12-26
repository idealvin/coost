file(GLOB TESTS *.cc)

include_directories(${CMAKE_SOURCE_DIR})

list(APPEND LIBS_TO_LINK base)
if(UNIX)
  list(APPEND LIBS_TO_LINK pthread dl)
endif()

list(APPEND TESTS_FILES
  co
  fast
  flag
  hash
  json
  rapidjson
  log
  rpc
  str
  time
  tw
  xx
  )
foreach(TEST ${TESTS_FILES})
  add_executable(${TEST} ${TEST}_test.cc)
  message("  - ${TEST}_test.cc -> ${TEST}")
  target_link_libraries(${TEST} 
    ${LIBS_TO_LINK}
    )
endforeach()
