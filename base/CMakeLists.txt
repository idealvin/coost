file(GLOB_RECURSE CC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

if(UNIX)
  add_library(base ${CC_FILES}
    co/context/context.S
    )
endif()

if(MSVC)
  file(GLOB_RECURSE CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
  set(ASM_FILES co/context/context_x86.asm)
  if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(ASM_FILES co/context/context_x64.asm)
  endif()
  set_property(SOURCE ${ASM_FILES} PROPERTY LANGUAGE ASM_MASM)
  add_library(base ${CC_FILES}
    ${CPP_FILES}
    ${ASM_FILES}
    )
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DESTINATION include/${PROJECT_NAME}
  FILES_MATCHING PATTERN "*.h"
  )
install(TARGETS base
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION lib/${PROJECT_NAME}  # dynamic lib installed to  ${CMAKE_INSTALL_PREFIX}/lib/co
  ARCHIVE DESTINATION lib/${PROJECT_NAME}  # static lib installed to   ${CMAKE_INSTALL_PREFIX}/lib/co
  RUNTIME DESTINATION bin                  # runtime installed to      ${CMAKE_INSTALL_PREFIX}/bin
  INCLUDES DESTINATION include/${PROJECT_NAME}
  )
install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Config.cmake
  DESTINATION lib/cmake/${PROJECT_NAME}
  )
## make a file that defines ${PROJECT_NAME}_INCLUDE_DIRS/${PROJECT_NAME}_LIBS etc.
file(WRITE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config-var.cmake
  "
get_filename_component(_PROJ_IMPORT_PREFIX \"\$\{CMAKE_CURRENT_LIST_FILE\}\" PATH)
get_filename_component(_PROJ_IMPORT_PREFIX \"\$\{_PROJ_IMPORT_PREFIX\}\" PATH)
get_filename_component(_PROJ_IMPORT_PREFIX \"\$\{_PROJ_IMPORT_PREFIX\}\" PATH)
get_filename_component(_PROJ_IMPORT_PREFIX \"\$\{_PROJ_IMPORT_PREFIX\}\" PATH)
if(_PROJ_IMPORT_PREFIX STREQUAL \"/\")
  set(_PROJ_IMPORT_PREFIX \"\")
endif()
list(APPEND ${PROJECT_NAME}_INCLUDE_DIR
  \$\{_PROJ_IMPORT_PREFIX\}/include/${PROJECT_NAME}
)
set(${PROJECT_NAME}_INCLUDE_DIRS \$\{${PROJECT_NAME}_INCLUDE_DIR\})

list(APPEND ${PROJECT_NAME}_LIBRARIES base)
if(UNIX)
  list(APPEND ${PROJECT_NAME}_LIBRARIES dl pthread)
endif()
set(${PROJECT_NAME}_LIBS \$\{${PROJECT_NAME}_LIBRARIES\})
set(${PROJECT_NAME}_LIBRARY \$\{${PROJECT_NAME}_LIBRARIES\})
")
install(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config-var.cmake
  DESTINATION lib/cmake/${PROJECT_NAME}
  )
