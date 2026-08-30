include(FetchContent)

set(PKMNRBL_NWII_ROOT "${CMAKE_CURRENT_LIST_DIR}/../third_party/NWiiRecomp")

foreach(required_path IN ITEMS
    "${PKMNRBL_NWII_ROOT}/nWiiAnalyzer"
    "${PKMNRBL_NWII_ROOT}/nWiiRecomp"
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Pinned NWiiRecomp source is missing: ${required_path}")
    endif()
endforeach()

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.28.5
)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL2)

# Revision 595f176f1d24cc54ff2e8389feed12d7fb553cc2 has no vendored GLAD
# directory.  Retain the upstream pinned fallback only in this dependency-
# enabled branch; asset-free configuration never includes this file.
set(NWII_VENDORED_GLAD "${PKMNRBL_NWII_ROOT}/nWiiRuntime/src/platform/glad")
if(EXISTS "${NWII_VENDORED_GLAD}/src/glad.c")
    add_library(glad STATIC "${NWII_VENDORED_GLAD}/src/glad.c")
    target_include_directories(glad PUBLIC "${NWII_VENDORED_GLAD}/include")
else()
    set(GLAD_PROFILE "core" CACHE STRING "" FORCE)
    set(GLAD_API "gl=3.3" CACHE STRING "" FORCE)
    set(GLAD_GENERATOR "c" CACHE STRING "" FORCE)
    FetchContent_Declare(
        glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG v0.1.36
    )
    FetchContent_MakeAvailable(glad)
endif()

file(GLOB_RECURSE NWII_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime/src/*.cpp")
list(REMOVE_ITEM NWII_RUNTIME_SOURCES
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime/src/core/main.cpp")

add_library(nwiiruntime STATIC ${NWII_RUNTIME_SOURCES})
target_include_directories(nwiiruntime PUBLIC
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include")
target_link_libraries(nwiiruntime PUBLIC
    pkmnrbl_build_options
    SDL2-static
    tomlplusplus::tomlplusplus
    glad)

if(APPLE)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(METAL_LIBRARY Metal REQUIRED)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    target_sources(nwiiruntime PRIVATE
        "${PKMNRBL_NWII_ROOT}/nWiiRuntime/src/hle/gx/renderer_metal.mm")
    target_link_libraries(nwiiruntime PUBLIC
        ${COCOA_LIBRARY}
        ${METAL_LIBRARY}
        ${QUARTZCORE_LIBRARY}
        ${IOKIT_LIBRARY}
        ${COREVIDEO_LIBRARY})
endif()

file(GLOB_RECURSE NWII_ANALYZER_SOURCES CONFIGURE_DEPENDS
    "${PKMNRBL_NWII_ROOT}/nWiiAnalyzer/src/*.cpp")
add_library(nwiianalyzer STATIC ${NWII_ANALYZER_SOURCES})
target_include_directories(nwiianalyzer PUBLIC
    "${PKMNRBL_NWII_ROOT}/nWiiAnalyzer/include"
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include"
    "${PKMNRBL_NWII_ROOT}/nWiiRecomp/include")
target_link_libraries(nwiianalyzer PUBLIC
    pkmnrbl_build_options
    nwiiruntime)

file(GLOB_RECURSE NWII_RECOMPILER_SOURCES CONFIGURE_DEPENDS
    "${PKMNRBL_NWII_ROOT}/nWiiRecomp/src/*.cpp")
list(REMOVE_ITEM NWII_RECOMPILER_SOURCES
    "${PKMNRBL_NWII_ROOT}/nWiiRecomp/src/main.cpp")
add_library(nwiirecomp_lib STATIC ${NWII_RECOMPILER_SOURCES})
target_include_directories(nwiirecomp_lib PUBLIC
    "${PKMNRBL_NWII_ROOT}/nWiiRecomp/include"
    "${PKMNRBL_NWII_ROOT}/nWiiAnalyzer/include"
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include")
target_link_libraries(nwiirecomp_lib PUBLIC
    pkmnrbl_build_options
    nwiianalyzer
    nwiiruntime)

add_executable(nwiirecomp "${PKMNRBL_NWII_ROOT}/nWiiRecomp/src/main.cpp")
target_link_libraries(nwiirecomp PRIVATE
    pkmnrbl_build_options
    nwiirecomp_lib
    tomlplusplus::tomlplusplus)
