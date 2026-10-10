if(NOT DEFINED PKMNRBL_NWII_RUNTIME_DIR)
    message(FATAL_ERROR "PKMNRBL_NWII_RUNTIME_DIR is required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/NWiiRecompSources.cmake")

pkmnrbl_collect_nwii_runtime_sources(nwii_runtime_sources "${PKMNRBL_NWII_RUNTIME_DIR}")

set(interpreter_source "${PKMNRBL_NWII_RUNTIME_DIR}/src/hle/interpreter.cpp")
list(FIND nwii_runtime_sources "${interpreter_source}" interpreter_source_index)
if(NOT interpreter_source_index EQUAL -1)
    message(FATAL_ERROR "Default nwiiruntime source selection includes the prohibited PPC interpreter: ${interpreter_source}")
endif()

set(native_only_source "${PKMNRBL_NWII_RUNTIME_DIR}/src/core/native_only.cpp")
list(FIND nwii_runtime_sources "${native_only_source}" native_only_source_index)
if(NOT native_only_source_index EQUAL -1)
    message(FATAL_ERROR "Default nwiiruntime source selection must not force native-only guards into interpreter-ON builds: ${native_only_source}")
endif()

foreach(required_runtime_name IN ITEMS loader event_scheduler guest_timing)
    set(required_runtime_source "${PKMNRBL_NWII_RUNTIME_DIR}/src/core/${required_runtime_name}.cpp")
    list(FIND nwii_runtime_sources "${required_runtime_source}" required_runtime_source_index)
    if(required_runtime_source_index EQUAL -1)
        message(FATAL_ERROR "Default nwiiruntime source selection lost a required runtime source: ${required_runtime_source}")
    endif()
endforeach()

message(STATUS "PASS: default nwiiruntime source selection excludes the PPC interpreter and retains the loader and guest timers.")
