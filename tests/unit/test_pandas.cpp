// The built-in `pandas` module (Series/DataFrame): a C++ integration test that the frozen Kirito
// module loads and its core surface works end-to-end. Exhaustive behavioral + fuzz coverage lives in
// tests/scripts/spec_pandas.ki; this guards the embedded module and the import path.
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

// Run a program that ends in an expression and return its stringified value. Each call uses a fresh
// VM so the cases stay independent.
static std::string run(const std::string& body) {
    KiritoVM vm;
    return vm.stringify(vm.runSource("var pd = import(\"pandas\")\n" + body));
}

int main() {
    // --- Series ---
    CHECK(run("pd.Series([1, 2, 3, 4]).sum()") == "10");
    CHECK(run("pd.Series([1, 2, 3, 4]).mean()") == "2.5");
    CHECK(run("pd.Series([3, 1, 2]).sortvalues().tolist()") == "[1, 2, 3]");
    CHECK(run("(pd.Series([1, 2, 3]) * 10).tolist()") == "[10, 20, 30]");
    CHECK(run("(pd.Series([1, 2, 3, 4]) > 2).tolist()") == "[False, False, True, True]");
    CHECK(run("pd.Series([1, None, 3]).sum()") == "4");          // missing skipped
    CHECK(run("pd.Series([1, None, 3]).dropna().tolist()") == "[1, 3]");

    // --- DataFrame construction + selection ---
    const char* mk = "var df = pd.DataFrame([[\"Ada\", 36], [\"Alan\", 41], [\"Grace\", 85]], columns=[\"name\", \"age\"])\n";
    CHECK(run(std::string(mk) + "df.shape()") == "[3, 2]");
    CHECK(run(std::string(mk) + "df.columns") == "[name, age]");
    CHECK(run(std::string(mk) + "df[\"age\"].tolist()") == "[36, 41, 85]");
    CHECK(run(std::string(mk) + "df.iloc[1][\"name\"]") == "Alan");
    CHECK(run(std::string(mk) + "df[df[\"age\"] > 40][\"name\"].tolist()") == "[Alan, Grace]");
    CHECK(run(std::string(mk) + "df.sortvalues(\"age\", ascending=False)[\"name\"].tolist()") == "[Grace, Alan, Ada]");

    // --- CSV round-trip + type inference ---
    CHECK(run("type(pd.readcsv(\"a,b\\n1,2.5\")[\"a\"][0])") == "Integer");
    CHECK(run("type(pd.readcsv(\"a,b\\n1,2.5\")[\"b\"][0])") == "Float");
    CHECK(run("pd.readcsv(\"x\\n1\\n2\\n3\")[\"x\"].sum()") == "6");

    // --- group-by ---
    const char* gb = "var df = pd.DataFrame({\"k\": [\"a\", \"b\", \"a\", \"b\"], \"v\": [1, 2, 3, 4]}, columns=[\"k\", \"v\"])\n"
                     "var g = df.groupby(\"k\")\n";
    CHECK(run(std::string(gb) + "g.sum()[\"v\"].tolist()") == "[4, 6]");
    CHECK(run(std::string(gb) + "g.mean()[\"v\"].tolist()") == "[2.0, 3.0]");
    CHECK(run(std::string(gb) + "g.size().tolist()") == "[2, 2]");

    // --- joins + concat ---
    const char* jn = "var l = pd.DataFrame([[1, \"x\"], [2, \"y\"]], columns=[\"id\", \"a\"])\n"
                     "var r = pd.DataFrame([[1, \"p\"], [3, \"q\"]], columns=[\"id\", \"b\"])\n";
    CHECK(run(std::string(jn) + "pd.merge(l, r, on=\"id\").shape()") == "[1, 3]");
    CHECK(run(std::string(jn) + "pd.merge(l, r, on=\"id\", how=\"left\")[\"b\"].tolist()") == "[p, None]");
    CHECK(run("pd.concat([pd.DataFrame([[1]], columns=[\"x\"]), pd.DataFrame([[2]], columns=[\"x\"])])[\"x\"].tolist()") == "[1, 2]");

    // --- missing-data on a frame ---
    CHECK(run("len(pd.readcsv(\"a,b\\n1,\\n2,3\").dropna())") == "1");
    CHECK(run("pd.readcsv(\"a,b\\n1,\\n2,3\").fillna(0)[\"b\"].tolist()") == "[0, 3]");

    // --- adversarial: ragged construction is rejected ---
    {
        KiritoVM vm;
        CHECK_THROWS(vm.runSource("var pd = import(\"pandas\")\npd.DataFrame({\"a\": [1, 2], \"b\": [1]}, columns=[\"a\", \"b\"])"));
        CHECK_THROWS(vm.runSource("var pd = import(\"pandas\")\npd.Series([1, 2, 3], index=[\"a\"])"));
    }

    return RUN_TESTS();
}
