include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/NWiiRecompSources.cmake")

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

pkmnrbl_collect_nwii_runtime_sources(
    NWII_RUNTIME_SOURCES
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime")

add_library(nwiiruntime STATIC ${NWII_RUNTIME_SOURCES})
target_sources(nwiiruntime PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../runtime/boot/wii_memory_layout.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../runtime/boot/nwii_guest_memory.cpp")
target_include_directories(nwiiruntime PUBLIC
    "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include"
    "${CMAKE_CURRENT_LIST_DIR}/..")
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

if(PKMNRBL_BUILD_BOOT_TESTS)
    add_test(NAME nwii_runtime_sources
        COMMAND "${CMAKE_COMMAND}"
            "-DPKMNRBL_NWII_RUNTIME_DIR=${PKMNRBL_NWII_ROOT}/nWiiRuntime"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/nwii_runtime_sources.Tests.cmake")
    add_executable(gl_test_context_tests tests/gl_test_context_tests.cpp)
    target_link_libraries(gl_test_context_tests PRIVATE glad)
    add_test(NAME gl_test_context COMMAND gl_test_context_tests)

    add_executable(aot_resume_fixture tests/aot_resume_fixture.cpp)
    target_link_libraries(aot_resume_fixture PRIVATE nwiirecomp_lib)
    add_executable(aot_title_roots_fixture tests/aot_title_roots_fixture.cpp)
    target_link_libraries(aot_title_roots_fixture PRIVATE nwiirecomp_lib tomlplusplus::tomlplusplus)
    add_executable(native_only_fixture tests/native_only_fixture.cpp)
    target_link_libraries(native_only_fixture PRIVATE nwiirecomp_lib)
    set(native_only_micro_source "${CMAKE_CURRENT_BINARY_DIR}/native_only_micro_generated.cpp")
    add_custom_command(OUTPUT "${native_only_micro_source}"
        COMMAND native_only_fixture "${native_only_micro_source}"
        DEPENDS native_only_fixture VERBATIM)
    # Each shared output has one generation owner; all compiling consumers
    # wait for it so parallel builds cannot regenerate files during a compile.
    add_custom_target(native_only_micro_generated DEPENDS "${native_only_micro_source}")
    foreach(layout split single)
        set(resume_dir "${CMAKE_CURRENT_BINARY_DIR}/aot_resume_${layout}")
        if(layout STREQUAL "split")
            set(resume_sources "${resume_dir}/main_output.cpp" "${resume_dir}/output_0.cpp")
        else()
            set(resume_sources "${resume_dir}/output.cpp")
        endif()
        add_custom_command(OUTPUT ${resume_sources}
            COMMAND aot_resume_fixture "${resume_dir}" ${layout}
            DEPENDS aot_resume_fixture VERBATIM)
        add_custom_target(aot_resume_${layout}_generated DEPENDS ${resume_sources})
        add_executable(aot_resume_${layout}_tests tests/aot_resume_tests.cpp ${resume_sources})
        add_dependencies(aot_resume_${layout}_tests aot_resume_${layout}_generated)
        target_include_directories(aot_resume_${layout}_tests PRIVATE "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include")
        target_compile_features(aot_resume_${layout}_tests PRIVATE cxx_std_20)
        add_test(NAME aot_resume_${layout} COMMAND aot_resume_${layout}_tests)

        add_executable(native_only_${layout}_tests tests/native_only_tests.cpp
            ${resume_sources} "${native_only_micro_source}"
            "${PKMNRBL_NWII_ROOT}/nWiiRuntime/src/core/native_only.cpp")
        add_dependencies(native_only_${layout}_tests
            aot_resume_${layout}_generated native_only_micro_generated)
        target_include_directories(native_only_${layout}_tests PRIVATE "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include")
        target_compile_features(native_only_${layout}_tests PRIVATE cxx_std_20)
        add_test(NAME native_only_${layout}_covered COMMAND native_only_${layout}_tests)
        foreach(mode step micro)
            add_test(NAME native_only_${layout}_${mode}
                COMMAND "${CMAKE_COMMAND}"
                    "-DTEST_EXECUTABLE=$<TARGET_FILE:native_only_${layout}_tests>"
                    "-DTEST_MODE=${mode}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_only_failfast.Tests.cmake")
        endforeach()

        set(roots_dir "${CMAKE_CURRENT_BINARY_DIR}/aot_title_roots_${layout}")
        if(layout STREQUAL "split")
            set(roots_sources "${roots_dir}/main_output.cpp" "${roots_dir}/output_0.cpp")
        else()
            set(roots_sources "${roots_dir}/output.cpp")
        endif()
        add_custom_command(OUTPUT ${roots_sources}
            COMMAND aot_title_roots_fixture "${roots_dir}" ${layout} config/WPSE01_01/recomp.toml
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            DEPENDS aot_title_roots_fixture "${CMAKE_CURRENT_SOURCE_DIR}/config/WPSE01_01/recomp.toml"
                "${CMAKE_CURRENT_SOURCE_DIR}/config/WPSE01_01/aot_roots.csv"
            VERBATIM)
        add_executable(aot_title_roots_${layout}_tests tests/aot_title_roots_tests.cpp ${roots_sources})
        target_include_directories(aot_title_roots_${layout}_tests PRIVATE "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include")
        target_compile_features(aot_title_roots_${layout}_tests PRIVATE cxx_std_20)
        add_test(NAME aot_title_roots_${layout} COMMAND aot_title_roots_${layout}_tests)
    endforeach()

    add_executable(tev_shader_gl_tests tests/tev_shader_gl_tests.cpp)
    target_compile_definitions(tev_shader_gl_tests PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(tev_shader_gl_tests PRIVATE nwiiruntime)
    add_test(NAME tev_shader_gl COMMAND tev_shader_gl_tests)
    set_tests_properties(tev_shader_gl PROPERTIES SKIP_RETURN_CODE 77)

    add_executable(aot_syscall_fixture tests/aot_syscall_fixture.cpp)
    target_link_libraries(aot_syscall_fixture PRIVATE nwiirecomp_lib)
    set(syscall_fixture "${CMAKE_CURRENT_BINARY_DIR}/aot_syscall_generated.cpp")
    add_custom_command(OUTPUT "${syscall_fixture}"
        COMMAND aot_syscall_fixture "${syscall_fixture}"
        DEPENDS aot_syscall_fixture VERBATIM)
    add_executable(aot_syscall_tests tests/aot_syscall_tests.cpp "${syscall_fixture}")
    target_include_directories(aot_syscall_tests PRIVATE "${PKMNRBL_NWII_ROOT}/nWiiRuntime/include")
    target_compile_features(aot_syscall_tests PRIVATE cxx_std_20)
    foreach(mode normal interrupt yield)
        add_test(NAME aot_syscall_${mode} COMMAND aot_syscall_tests ${mode})
    endforeach()
endif()
