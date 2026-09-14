file(GLOB SRC_FILES *.cc)

add_executable(benchmark ${SRC_FILES})
target_link_libraries(benchmark PRIVATE co)
