cmake_minimum_required(VERSION 3.25)

option(PKMNRBL_BUILD_TOOLS "Build NWiiRecomp host tooling and runtime targets" ON)
option(PKMNRBL_FETCH_DEPS "Fetch pinned third-party build dependencies" ON)

set(PKMNRBL_BUILD_BOOT_TESTS_DEFAULT OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PKMNRBL_BUILD_BOOT_TESTS_DEFAULT ON)
endif()
option(PKMNRBL_BUILD_BOOT_TESTS "Build standalone Wii boot-memory tests" ${PKMNRBL_BUILD_BOOT_TESTS_DEFAULT})

# The asset-free configuration is deliberately compiler-independent.  Runtime
# and host-tool sources require SDL/OpenGL dependencies, so they are added only
# when both source targets and dependency fetching have been requested.
if(PKMNRBL_BUILD_TOOLS AND PKMNRBL_FETCH_DEPS)
    project(pkmnrbl LANGUAGES C CXX)
elseif(PKMNRBL_BUILD_TOOLS OR PKMNRBL_BUILD_BOOT_TESTS)
    project(pkmnrbl LANGUAGES CXX)
else()
    project(pkmnrbl LANGUAGES NONE)
endif()

if(PKMNRBL_BUILD_BOOT_TESTS)
    enable_testing()
    add_library(wii_memory_layout OBJECT runtime/boot/wii_memory_layout.cpp)
    target_include_directories(wii_memory_layout PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(wii_memory_layout PUBLIC cxx_std_20)

    add_executable(wii_memory_layout_tests tests/wii_memory_layout_tests.cpp)
    target_include_directories(wii_memory_layout_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(wii_memory_layout_tests PRIVATE cxx_std_20)
    target_link_libraries(wii_memory_layout_tests PRIVATE wii_memory_layout)
    add_test(NAME wii_memory_layout COMMAND wii_memory_layout_tests)

    add_executable(nwii_boot_integration_tests
        tests/nwii_boot_integration_tests.cpp
        runtime/boot/nwii_guest_memory.cpp)
    target_include_directories(nwii_boot_integration_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(nwii_boot_integration_tests PRIVATE cxx_std_20)
    target_link_libraries(nwii_boot_integration_tests PRIVATE wii_memory_layout)
    add_test(NAME nwii_boot_integration COMMAND nwii_boot_integration_tests)

    add_executable(ipc_async_reply_tests
        tests/ipc_async_reply_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_ipc.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/mmio_dispatcher.cpp)
    target_include_directories(ipc_async_reply_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(ipc_async_reply_tests PRIVATE cxx_std_20)
    add_test(NAME ipc_async_reply COMMAND ipc_async_reply_tests)
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
