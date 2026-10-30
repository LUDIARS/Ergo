# Validates a Chrome Trace Event file written by `ergo::profile::dump`.
#   cmake -DTRACE=<file> -P check_trace.cmake
# Fails unless the file parses as JSON, has a non-empty `traceEvents` array
# and contains the `frame` scope marker the example records every frame.

if(NOT DEFINED TRACE)
    message(FATAL_ERROR "check_trace: pass -DTRACE=<file>")
endif()
if(NOT EXISTS "${TRACE}")
    message(FATAL_ERROR "check_trace: ${TRACE} does not exist")
endif()

file(READ "${TRACE}" _json)
string(JSON _count ERROR_VARIABLE _err LENGTH "${_json}" traceEvents)
if(_err)
    message(FATAL_ERROR "check_trace: ${TRACE} is not a Chrome trace: ${_err}")
endif()
if(_count LESS 1)
    message(FATAL_ERROR "check_trace: ${TRACE} has an empty traceEvents array")
endif()

# Parsing above proved the structure; a plain search finds the marker.
string(FIND "${_json}" "\"name\":\"frame\"" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR "check_trace: ${TRACE} has no 'frame' markers")
endif()
message(STATUS "check_trace: ${TRACE}: ${_count} events")
