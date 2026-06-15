# Run a .ki script through `ki` and compare stdout to a golden .expected file.
# Args (via -D): KI (interpreter), SCRIPT, EXPECTED, optional INPUT (stdin file),
# optional ARGS (semicolon-separated extra script argv tokens after the script path).
if(DEFINED ARGS AND NOT ARGS STREQUAL "")
    set(_script_args ${ARGS})
else()
    set(_script_args)
endif()
if(DEFINED INPUT AND NOT INPUT STREQUAL "")
    execute_process(COMMAND ${KI} ${SCRIPT} ${_script_args} INPUT_FILE ${INPUT}
                    OUTPUT_VARIABLE actual RESULT_VARIABLE rc)
else()
    execute_process(COMMAND ${KI} ${SCRIPT} ${_script_args}
                    OUTPUT_VARIABLE actual RESULT_VARIABLE rc)
endif()

file(READ ${EXPECTED} expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR
        "Script ${SCRIPT} output mismatch (exit ${rc}).\n"
        "--- expected ---\n${expected}\n--- actual ---\n${actual}")
endif()
