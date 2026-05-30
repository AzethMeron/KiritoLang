#ifndef KIRITO_HPP
#define KIRITO_HPP

// Umbrella header — `#include "kirito.hpp"` to embed Kirito in your own C++ program.
// This pulls in everything needed to construct a KiritoVM and run Kirito source. It does
// NOT define main(); the standalone interpreter lives in main.cpp.

#include "kirito/common.hpp"
#include "kirito/control.hpp"
#include "kirito/exceptions.hpp"
#include "kirito/handle.hpp"
#include "kirito/object.hpp"
#include "kirito/arena.hpp"
#include "kirito/builtins.hpp"
#include "kirito/collections.hpp"
#include "kirito/module.hpp"
#include "kirito/ast.hpp"
#include "kirito/lexer.hpp"
#include "kirito/parser.hpp"
#include "kirito/environment.hpp"
#include "kirito/function.hpp"
#include "kirito/vm.hpp"
#include "kirito/native.hpp"
#include "kirito/stdlib_io.hpp"
#include "kirito/evaluator.hpp"
#include "kirito/runtime.hpp"

#endif
