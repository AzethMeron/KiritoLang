# One small executable per test file (one behavior-area each), linking the header-only core.
find_package(Threads REQUIRED)

# Shared precompiled header. Every test TU does `#include "kirito.hpp"`, which pulls in the entire
# header-only interpreter — parsing it once per TU dominates the build. This carrier compiles that
# umbrella into a PCH ONCE; each test reuses it (REUSE_FROM below), so a clean build parses the core
# a single time instead of ~60 times. The carrier carries the exact compile setup the tests use
# (same warnings, same kirito/Threads usage requirements) so the PCH is binary-compatible to reuse.
add_library(kirito_pch STATIC pch_carrier.cpp)
target_link_libraries(kirito_pch PRIVATE kirito Threads::Threads)
target_compile_options(kirito_pch PRIVATE -Wall -Wextra -Wpedantic)
target_precompile_headers(kirito_pch PRIVATE ${CMAKE_SOURCE_DIR}/src/kirito.hpp)

function(kirito_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE kirito Threads::Threads)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    target_precompile_headers(${name} REUSE_FROM kirito_pch)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kirito_test(test_arena unit/test_arena.cpp)
kirito_test(test_lexer unit/test_lexer.cpp)
kirito_test(test_cli_paths unit/test_cli_paths.cpp)
kirito_test(test_eval_arith unit/test_eval_arith.cpp)
kirito_test(test_eval_vars unit/test_eval_vars.cpp)
kirito_test(test_indent unit/test_indent.cpp)
kirito_test(test_control unit/test_control.cpp)
kirito_test(test_conditional unit/test_conditional.cpp)
kirito_test(test_functions unit/test_functions.cpp)
kirito_test(test_collections unit/test_collections.cpp)
kirito_test(test_modules unit/test_modules.cpp)
kirito_test(test_strings unit/test_strings.cpp)
kirito_test(test_string_literals unit/test_string_literals.cpp)
kirito_test(test_gc unit/test_gc.cpp)
kirito_test(test_exceptions unit/test_exceptions.cpp)
kirito_test(test_exceptions_cpp unit/test_exceptions_cpp.cpp)
kirito_test(test_classes unit/test_classes.cpp)
kirito_test(test_kwargs unit/test_kwargs.cpp)
kirito_test(test_tabular unit/test_tabular.cpp)
kirito_test(test_xml unit/test_xml.cpp)
kirito_test(test_audit_fixes unit/test_audit_fixes.cpp)
kirito_test(test_audit_findings unit/test_audit_findings.cpp)
kirito_test(test_protocol unit/test_protocol.cpp)
kirito_test(test_value_extra unit/test_value_extra.cpp)
kirito_test(test_embedding_extra unit/test_embedding_extra.cpp)
kirito_test(test_serde unit/test_serde.cpp)
kirito_test(test_serialize_native unit/test_serialize_native.cpp)
kirito_test(test_with unit/test_with.cpp)
kirito_test(test_string_ops unit/test_string_ops.cpp)
kirito_test(test_stdlib unit/test_stdlib.cpp)
kirito_test(test_extras unit/test_extras.cpp)
kirito_test(test_fstring unit/test_fstring.cpp)
kirito_test(test_references unit/test_references.cpp)
kirito_test(test_imports unit/test_imports.cpp)
kirito_test(test_builtins unit/test_builtins.cpp)
kirito_test(test_random unit/test_random.cpp)
kirito_test(test_matrix unit/test_matrix.cpp)
kirito_test(test_complex unit/test_complex.cpp)
kirito_test(test_tensor unit/test_tensor.cpp)
kirito_test(test_multi_index unit/test_multi_index.cpp)
kirito_test(test_class_ops unit/test_class_ops.cpp)
kirito_test(test_json unit/test_json.cpp)
kirito_test(test_net unit/test_net.cpp)
kirito_test(test_serialize unit/test_serialize.cpp)
kirito_test(test_sys unit/test_sys.cpp)
kirito_test(test_time unit/test_time.cpp)
kirito_test(test_dump unit/test_dump.cpp)
kirito_test(test_zlib unit/test_zlib.cpp)
kirito_test(test_bytes unit/test_bytes.cpp)
kirito_test(test_apply unit/test_apply.cpp)
kirito_test(test_hash unit/test_hash.cpp)
kirito_test(test_regex unit/test_regex.cpp)
kirito_test(test_regex_corpus unit/test_regex_corpus.cpp)
kirito_test(test_sort unit/test_sort.cpp)
kirito_test(test_hostile_libs unit/test_hostile_libs.cpp)
kirito_test(test_io unit/test_io.cpp)
kirito_test(test_namedargs unit/test_namedargs.cpp)
kirito_test(test_adversarial unit/test_adversarial.cpp)
kirito_test(test_unpacking unit/test_unpacking.cpp)
kirito_test(test_stdmodules unit/test_stdmodules.cpp)
kirito_test(test_semver unit/test_semver.cpp)
kirito_test(test_kpm unit/test_kpm.cpp)
# kpm.ki lives at the repo root, not under tests/; hand its directory to the test via the environment
# so `import("kpm")` resolves it (passed by env, not a -D, to keep the shared PCH reusable).
set_tests_properties(test_kpm PROPERTIES ENVIRONMENT "KPM_DIR=${CMAKE_SOURCE_DIR}/kpm")
kirito_test(test_introspect unit/test_introspect.cpp)
kirito_test(test_warnings unit/test_warnings.cpp)
kirito_test(test_unicode unit/test_unicode.cpp)
kirito_test(test_hardening unit/test_hardening.cpp)
kirito_test(test_hardening2 unit/test_hardening2.cpp)
kirito_test(test_stdlib_extra unit/test_stdlib_extra.cpp)
kirito_test(test_super unit/test_super.cpp)
kirito_test(test_stdlib_gaps unit/test_stdlib_gaps.cpp)
kirito_test(test_list_ops unit/test_list_ops.cpp)
kirito_test(test_bytesio unit/test_bytesio.cpp)
kirito_test(test_switch unit/test_switch.cpp)
kirito_test(test_streams unit/test_streams.cpp)
kirito_test(test_stress unit/test_stress.cpp)
kirito_test(test_value unit/test_value.cpp)
kirito_test(test_modules_fuzz unit/test_modules_fuzz.cpp)

# Multiprocessing (KiritoDispatcher + the `parallel` module). Every blocking scenario runs under an
# in-test deadlock watchdog; the CTest TIMEOUT is a coarse backstop so a regression that slips past
# the watchdog still fails the suite rather than stalling CI.
kirito_test(test_parallel unit/test_parallel.cpp)
kirito_test(test_parallel_sync unit/test_parallel_sync.cpp)
kirito_test(test_parallel_deadlock unit/test_parallel_deadlock.cpp)
kirito_test(test_parallel_random unit/test_parallel_random.cpp)
kirito_test(test_parallel_edge unit/test_parallel_edge.cpp)
kirito_test(test_parallel_fuzz unit/test_parallel_fuzz.cpp)
set_tests_properties(test_parallel test_parallel_sync test_parallel_deadlock test_parallel_random
                     test_parallel_edge test_parallel_fuzz PROPERTIES TIMEOUT 120)

kirito_test(embed_demo integration/embed_demo.cpp)
kirito_test(fuzz_eval fuzz/fuzz_eval.cpp)
kirito_test(bench bench/bench.cpp)

# --- end-to-end .ki script tests (run through the `ki` interpreter, diff vs .expected) ---
# A .ki file is only registered as a test if it has a matching .expected; this lets library
# files (imported by other scripts, e.g. complex.ki) live alongside without being run directly.
file(GLOB ki_scripts ${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.ki)
foreach(script ${ki_scripts})
    get_filename_component(nm ${script} NAME_WE)
    set(expected ${CMAKE_CURRENT_SOURCE_DIR}/scripts/${nm}.expected)
    if(NOT EXISTS ${expected})
        continue()
    endif()
    set(input ${CMAKE_CURRENT_SOURCE_DIR}/scripts/${nm}.in)
    set(extra "")
    if(EXISTS ${input})
        set(extra -DINPUT=${input})
    endif()
    add_test(NAME script_${nm}
             COMMAND ${CMAKE_COMMAND} -DKI=$<TARGET_FILE:ki> -DSCRIPT=${script}
                     -DEXPECTED=${expected} ${extra}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/run_ki_script.cmake)
endforeach()

# The parallel golden scripts can block on a real bug; cap them so a regression fails rather than
# stalling CI (the watchdog backstop, mirroring the parallel unit tests' TIMEOUT).
foreach(nm parallel_basic parallel_sync probe_parallel probe_parallel_adversarial)
    if(TEST script_${nm})
        set_tests_properties(script_${nm} PROPERTIES TIMEOUT 60)
    endif()
endforeach()

# --- error-message tests: .ki files that MUST fail, with a .experr of required diagnostic text ---
# Verifies the interpreter reports actionable errors (syntax, indentation, runtime, annotations)
# rather than crashing or going silent. Auto-discovered, so new cases need no CMake edits.
file(GLOB ki_error_scripts ${CMAKE_CURRENT_SOURCE_DIR}/errors/*.ki)
foreach(script ${ki_error_scripts})
    get_filename_component(nm ${script} NAME_WE)
    set(expected ${CMAKE_CURRENT_SOURCE_DIR}/errors/${nm}.experr)
    if(NOT EXISTS ${expected})
        continue()
    endif()
    add_test(NAME error_${nm}
             COMMAND ${CMAKE_COMMAND} -DKI=$<TARGET_FILE:ki> -DSCRIPT=${script}
                     -DEXPECTED=${expected}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/run_ki_error.cmake)
endforeach()

# --- the `imaging` example package (examples/big_projects/imaging): run its self-test through ki ---
# A pure-Kirito Pillow-style image library built on the tensor backend. Its modules resolve via the
# script's own directory (on the import path), so no copies are needed here. The self-test is an
# assert-driven .ki whose final lines are matched against test_imaging.expected.
set(_imaging_dir ${CMAKE_SOURCE_DIR}/examples/big_projects/imaging)
add_test(NAME script_imaging
         COMMAND ${CMAKE_COMMAND} -DKI=$<TARGET_FILE:ki>
                 -DSCRIPT=${_imaging_dir}/test_imaging.ki
                 -DEXPECTED=${_imaging_dir}/test_imaging.expected
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/run_ki_script.cmake)

# The video module (jpeg/gif codecs + an OpenCV-style VideoCapture) over the committed testdata/
# assets. Run with the package dir as the working directory so the `testdata/...` paths resolve.
add_test(NAME script_video
         COMMAND ${CMAKE_COMMAND} -DKI=$<TARGET_FILE:ki>
                 -DSCRIPT=${_imaging_dir}/test_video.ki
                 -DEXPECTED=${_imaging_dir}/test_video.expected
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/run_ki_script.cmake)
set_tests_properties(script_video PROPERTIES WORKING_DIRECTORY ${_imaging_dir})

# --- the self-host interpreter: run the main .ki suite THROUGH a Kirito-in-Kirito interpreter ---
# examples/big_projects/selfhost lexes/parses/evaluates every tools/tests/scripts program and checks
# it reproduces the recorded output (the language's most demanding stress test). run_tests.ki exits
# non-zero if any program diverges. Run from the repo root so its relative paths resolve. Slow by
# design (a tree-walker on a tree-walker), hence a generous timeout; pass `full` to also run the
# SLOW crypto/compression programs (kept out of the default run for tractability).
add_test(NAME selfhost
         COMMAND $<TARGET_FILE:ki> --lib ${CMAKE_SOURCE_DIR}/examples/big_projects/selfhost/lib
                 ${CMAKE_SOURCE_DIR}/examples/big_projects/selfhost/run_tests.ki)
set_tests_properties(selfhost PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 2400)
