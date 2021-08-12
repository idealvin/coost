file(GLOB SRC_FILES *.cc)

add_executable(gen ${SRC_FILES})

list(APPEND LIBS_TO_LINK co)

target_link_libraries(gen
    ${LIBS_TO_LINK}
)
