cmake_minimum_required(VERSION 3.25)

option(PKMNRBL_BUILD_TOOLS "Build NWiiRecomp host tooling and runtime targets" ON)
option(PKMNRBL_FETCH_DEPS "Fetch pinned third-party build dependencies" ON)

# The asset-free configuration is deliberately compiler-independent.  Runtime
# and host-tool sources require SDL/OpenGL dependencies, so they are added only
# when both source targets and dependency fetching have been requested.
if(PKMNRBL_BUILD_TOOLS AND PKMNRBL_FETCH_DEPS)
    project(pkmnrbl LANGUAGES C CXX)
elseif(PKMNRBL_BUILD_TOOLS)
    project(pkmnrbl LANGUAGES CXX)
else()
    project(pkmnrbl LANGUAGES NONE)
endif()

add_library(pkmnrbl_build_options INTERFACE)
if(PKMNRBL_BUILD_TOOLS)
    target_compile_features(pkmnrbl_build_options INTERFACE cxx_std_20)

    if(MSVC)
        target_compile_options(pkmnrbl_build_options INTERFACE /W4 /permissive- /EHsc)
    endif()
endif()

function(pkmnrbl_enable_bigobj target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot add /bigobj: target does not exist: ${target_name}")
    endif()

    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /bigobj)
    endif()
endfunction()

if(PKMNRBL_BUILD_TOOLS AND PKMNRBL_FETCH_DEPS)
    include(cmake/NWiiRecomp.cmake)
elseif(PKMNRBL_BUILD_TOOLS)
    message(STATUS "PKMNRBL_FETCH_DEPS=OFF: runtime and host tools are disabled; standalone boot targets may be added without SDL/OpenGL dependencies.")
else()
    message(STATUS "Asset-free configuration: host tools, runtime, and fetched dependencies are disabled.")
endif()
