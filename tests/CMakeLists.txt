# One small executable per test file (one behavior-area each), linking the header-only core.
function(kirito_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE kirito)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

kirito_test(test_arena unit/test_arena.cpp)
kirito_test(test_lexer unit/test_lexer.cpp)
kirito_test(test_eval_arith unit/test_eval_arith.cpp)
