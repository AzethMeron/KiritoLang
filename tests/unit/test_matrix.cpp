#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string evalStr(KiritoVM& vm, const std::string& src) {
    return vm.stringify(vm.runSource(src));
}

static const char* P = "var matrix = import(\"matrix\")\nvar A = matrix.Matrix([[1, 2], [3, 4]])\n"
                       "var B = matrix.Matrix([[5, 6], [7, 8]])\n";

int main() {
    KiritoVM vm;

    // construction and string form (elements are doubles)
    CHECK(evalStr(vm, std::string(P) + "A") == "[[1.0, 2.0], [3.0, 4.0]]");
    CHECK(evalStr(vm, std::string(P) + "A.shape()") == "[2, 2]");
    CHECK(evalStr(vm, std::string(P) + "A.rows()") == "2");

    // arithmetic
    CHECK(evalStr(vm, std::string(P) + "A + B") == "[[6.0, 8.0], [10.0, 12.0]]");
    CHECK(evalStr(vm, std::string(P) + "B - A") == "[[4.0, 4.0], [4.0, 4.0]]");
    CHECK(evalStr(vm, std::string(P) + "A * B") == "[[19.0, 22.0], [43.0, 50.0]]");
    CHECK(evalStr(vm, std::string(P) + "A * 2") == "[[2.0, 4.0], [6.0, 8.0]]");

    // transpose, determinant, trace
    CHECK(evalStr(vm, std::string(P) + "A.transpose()") == "[[1.0, 3.0], [2.0, 4.0]]");
    CHECK(evalStr(vm, std::string(P) + "A.determinant()") == "-2.0");
    CHECK(evalStr(vm, std::string(P) + "A.trace()") == "5.0");

    // inverse: A * inv(A) == I (within tolerance, via Matrix equality)
    CHECK(evalStr(vm, std::string(P) + "A * A.inverse() == matrix.identity(2)") == "True");
    CHECK(evalStr(vm, "var m = import(\"matrix\")\nm.identity(3)") ==
          "[[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]");

    // a known 3x3 determinant
    CHECK(evalStr(vm, "import(\"matrix\").Matrix([[2, 0, 0], [0, 3, 0], [0, 0, 4]]).determinant()") == "24.0");

    // factories, apply, get/set
    CHECK(evalStr(vm, "import(\"matrix\").zeros(2, 3)") == "[[0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]");
    CHECK(evalStr(vm, std::string(P) + "A.apply(Function(x): return x + 10)") ==
          "[[11.0, 12.0], [13.0, 14.0]]");
    CHECK(evalStr(vm, std::string(P) + "A.get(1, 0)") == "3.0");
    CHECK(evalStr(vm, std::string(P) + "A.set(0, 0, 99)\nA.get(0, 0)") == "99.0");
    CHECK(evalStr(vm, std::string(P) + "A.row(1)") == "[3.0, 4.0]");
    CHECK(evalStr(vm, std::string(P) + "A.sum()") == "10.0");

    // error cases
    CHECK_THROWS(vm.runSource(std::string(P) + "matrix.Matrix([[1, 2], [3, 4], [5, 6]]).inverse()\n"));  // non-square
    CHECK_THROWS(vm.runSource("import(\"matrix\").Matrix([[1, 2]]) + import(\"matrix\").Matrix([[1], [2]])\n"));  // shape
    CHECK_THROWS(vm.runSource("import(\"matrix\").Matrix([[1, 1], [1, 1]]).inverse()\n"));  // singular
    CHECK_THROWS(vm.runSource("import(\"matrix\").Matrix([[1, 2], [3]])\n"));  // ragged

    return RUN_TESTS();
}
