function(pkmnrbl_collect_nwii_runtime_sources output_variable runtime_directory)
    if(DEFINED CMAKE_SCRIPT_MODE_FILE)
        file(GLOB_RECURSE runtime_sources
            "${runtime_directory}/src/*.cpp")
    else()
        file(GLOB_RECURSE runtime_sources CONFIGURE_DEPENDS
            "${runtime_directory}/src/*.cpp")
    endif()
    list(REMOVE_ITEM runtime_sources
        "${runtime_directory}/src/core/main.cpp"
        "${runtime_directory}/src/hle/interpreter.cpp")
    set(${output_variable} "${runtime_sources}" PARENT_SCOPE)
endfunction()
