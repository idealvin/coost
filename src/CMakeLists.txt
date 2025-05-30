# Build co library
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

add_library(co ${SRC_FILES})
add_library(coost::co ALIAS co)

target_include_directories(co
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(co PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_features(co PUBLIC cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(co
    PUBLIC Threads::Threads
    PRIVATE ${CMAKE_DL_LIBS}
)
