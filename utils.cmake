#Redefine the __FILE__ macro of the source file of the current target
function(redefine_file_macro targetname)
    #Get all source files of the current target
    get_target_property(source_files "${targetname}" SOURCES)
    #Traverse source files
    foreach(sourcefile ${source_files})
        #Get the compilation parameters of the current source file
        get_property(defs SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS)
        #Get the absolute path of the current file
        get_filename_component(filepath "${sourcefile}" ABSOLUTE)
        #Replace the project path in the absolute path with empty to get the relative path of the source file relative to the project path
        message("psd: ${PROJECT_SOURCE_DIR}, fp: ${filepath}")
        string(REPLACE ${PROJECT_SOURCE_DIR}/ "" relpath ${filepath})
        #Add the compilation parameters (__FILE__ definition) we want to add to the original compilation parameters
        list(APPEND defs "__FILE__=\"${relpath}\"")
        #Reset the compilation parameters of the source file
        set_property(
            SOURCE "${sourcefile}"
            PROPERTY COMPILE_DEFINITIONS ${defs}
            )
    endforeach()
endfunction()