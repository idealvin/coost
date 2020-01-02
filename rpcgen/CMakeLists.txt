file(GLOB SRC_FILES *.cc)
include_directories(${CMAKE_SOURCE_DIR})

add_executable(rpcgen ${SRC_FILES})
list(APPEND LIBS_TO_LINK base)
if(UNIX)
  list(APPEND LIBS_TO_LINK pthread dl)
endif()
target_link_libraries(rpcgen
  ${LIBS_TO_LINK}
  )
install(TARGETS rpcgen
  RUNTIME DESTINATION bin
  )
