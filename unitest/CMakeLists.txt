file(GLOB SRC_FILES *.cc)

add_executable(unitest ${SRC_FILES})

list(APPEND LIBS_TO_LINK co)

target_link_libraries(unitest co
    ${LIBS_TO_LINK}
)
