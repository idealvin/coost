file(GLOB SRC_FILES *.cc)

add_executable(gen ${SRC_FILES})
target_link_libraries(gen PRIVATE co)
