file(GLOB SRC_FILES *.cc)
include_directories(${CMAKE_SOURCE_DIR})

add_executable(unitest ${SRC_FILES})
list(APPEND LIBS_TO_LINK base)
if(UNIX)
  list(APPEND LIBS_TO_LINK pthread dl)
endif()
target_link_libraries(unitest
  ${LIBS_TO_LINK}
  )
