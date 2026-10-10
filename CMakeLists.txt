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

    add_executable(title_identity_tests tests/title_identity_tests.cpp)
    target_include_directories(title_identity_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(title_identity_tests PRIVATE cxx_std_20)
    add_test(NAME title_identity COMMAND title_identity_tests)

    add_executable(es_content_tests
        tests/es_content_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/devices/es_content.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/devices/es_device.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/ios_kernel.cpp)
    target_include_directories(es_content_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(es_content_tests PRIVATE cxx_std_20)
    add_test(NAME es_content COMMAND es_content_tests)

    add_executable(sysconf_tests
        tests/sysconf_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/devices/fs_device.cpp)
    target_include_directories(sysconf_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(sysconf_tests PRIVATE cxx_std_20)
    add_test(NAME sysconf COMMAND sysconf_tests)

    add_executable(native_wii_input_tests
        tests/native_wii_input_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/pad.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/input/input_manager.cpp)
    target_include_directories(native_wii_input_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(native_wii_input_tests PRIVATE cxx_std_20)
    add_test(NAME native_wii_input COMMAND native_wii_input_tests)

    add_executable(default_controls_tests
        tests/default_controls_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/input/sources/mouse_keyboard_source.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/input/input_manager.cpp)
    target_include_directories(default_controls_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/input_host"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/src/input/sources")
    target_compile_features(default_controls_tests PRIVATE cxx_std_20)
    add_test(NAME default_controls COMMAND default_controls_tests)

    add_executable(ai_mmio_tests
        tests/ai_mmio_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_ai.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/mmio_dispatcher.cpp)
    target_include_directories(ai_mmio_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(ai_mmio_tests PRIVATE cxx_std_20)
    add_test(NAME ai_mmio COMMAND ai_mmio_tests)

    add_executable(guest_timing_tests
        tests/guest_timing_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/core/guest_timing.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/core/event_scheduler.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_vi.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/mmio_dispatcher.cpp)
    target_include_directories(guest_timing_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(guest_timing_tests PRIVATE cxx_std_20)
    foreach(case IN ITEMS presentations_120 presentations_240 presentations_120_wii presentations_240_wii
                          idle idle_wii deadlines busy coalescing wall reset masked_idle scheduler_deadlines
                          sdk_idle sdk_deadlines sdk_invalid sdk_guards
                          cpu_timebase cpu_dec cpu_event cpu_budget cpu_idle_inverse cpu_idle_limits)
        add_test(NAME guest_timing_${case} COMMAND guest_timing_tests ${case})
    endforeach()
    set_tests_properties(guest_timing_wall PROPERTIES ENVIRONMENT "NWII_WALLTB=1")

    add_executable(timing_trace_tests
        tests/timing_trace_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/core/os_hle_stubs.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/core/event_scheduler.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_dsp.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_ai.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/hw_vi.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hw/mmio_dispatcher.cpp)
    target_include_directories(timing_trace_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(timing_trace_tests PRIVATE cxx_std_20)
    foreach(case IN ITEMS fields dma mail ai)
        add_test(NAME timing_trace_${case} COMMAND timing_trace_tests ${case})
        set_tests_properties(timing_trace_${case} PROPERTIES ENVIRONMENT "NWII_TIMING_TRACE=1")
    endforeach()
    add_test(NAME timing_trace_disabled COMMAND timing_trace_tests disabled)
    set_tests_properties(timing_trace_disabled PROPERTIES
        ENVIRONMENT_MODIFICATION "NWII_TIMING_TRACE=unset:")

    add_executable(renderer_diagnostics_tests
        tests/renderer_diagnostics_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/gx/renderer_diagnostics.cpp)
    target_include_directories(renderer_diagnostics_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(renderer_diagnostics_tests PRIVATE cxx_std_20)
    add_test(NAME renderer_diagnostics COMMAND renderer_diagnostics_tests)

    add_executable(gx_display_list_memory_tests
        tests/gx_display_list_memory_tests.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/gx.cpp
        third_party/NWiiRecomp/nWiiRuntime/src/hle/gx/fifo_parser.cpp)
    target_include_directories(gx_display_list_memory_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NWiiRecomp/nWiiRuntime/include")
    target_compile_features(gx_display_list_memory_tests PRIVATE cxx_std_20)
    add_test(NAME gx_display_list_memory COMMAND gx_display_list_memory_tests)
    add_test(NAME gx_decode_diagnostic COMMAND gx_display_list_memory_tests decode-diagnostic)
    add_test(NAME gx_decode_diagnostic_disabled COMMAND gx_display_list_memory_tests decode-diagnostic)
    set_tests_properties(gx_display_list_memory gx_decode_diagnostic gx_decode_diagnostic_disabled
        PROPERTIES ENVIRONMENT_MODIFICATION
        "NWII_NODLCACHE=unset:;NWII_NOSEED=unset:;NWII_GXTRACE=unset:;NWII_TEVTRACE=unset:;NWII_TEXMTXDBG=unset:;NWII_SKINDBG=unset:;NWII_DLDBG=unset:;NWII_XFDBG=unset:;NWII_VATDBG=unset:;NWII_DESYNC=unset:;NWII_SAMPLE=unset:;NWII_FRAMESIG=unset:;NWII_GXDUMP=unset:;NWII_WGSEG=unset:;NWII_WGTRACE=unset:;NWII_RENDER_DIAG=unset:")
    set_property(TEST gx_decode_diagnostic APPEND PROPERTY ENVIRONMENT_MODIFICATION
        "NWII_RENDER_DIAG=set:1")
    set_tests_properties(gx_decode_diagnostic PROPERTIES
        PASS_REGULAR_EXPRESSION "\\[GXDECODE\\].*input=3 decoded=3.*object_min=2,3,4 object_max=4,5,6 object_zero=0.*matrix=bank:3,single:0,defer:0 matrix_zero=3 first_translation=0,0,0 first_linear_maxabs=0.*decoded_min=0,0,0 decoded_max=0,0,0 pre_xf=3"
        FAIL_REGULAR_EXPRESSION "FAIL")
    set_tests_properties(gx_decode_diagnostic_disabled PROPERTIES
        FAIL_REGULAR_EXPRESSION "\\[GXDECODE\\];FAIL")

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
