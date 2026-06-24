// Tests kpm's pure (network-free) helper logic by importing kpm.ki as a module. Its command dispatch
// is guarded by `if argmain:`, so importing only DEFINES the helpers (no commands run, no network).
// The semantic-versioning core kpm builds on is covered exhaustively by test_semver; resolveRef's
// network path (tags -> maxsatisfying) is exercised against the live GitHub API only in real use.
//
// The kpm/ directory is supplied at runtime via $KPM_DIR (set by CTest), put on the import path so
// `import("kpm")` resolves kpm.ki — passed by env, not a -D define, to keep the shared PCH reusable.
#include <cstdlib>
#include <string>

#include "../check.hpp"
#include "kirito.hpp"

using namespace kirito;

static std::string kpmDir() {
    const char* d = std::getenv("KPM_DIR");
    return d && *d ? std::string(d) : std::string(".");
}

// Evaluate an expression against a freshly-imported `kpm`, returning the stringified result.
static std::string ev(const std::string& expr) {
    KiritoVM vm;
    vm.addLibPath(kpmDir());
    return vm.stringify(vm.runSource("var kpm = import(\"kpm\")\n" + expr + "\n"));
}

int main() {
    // ---------- parseSpec: owner/repo[@ref]; ref is None / a literal ref / a constraint ----------
    CHECK(ev("String(kpm.parseSpec(\"owner/repo\"))") == "['owner', 'repo', None]");
    CHECK(ev("String(kpm.parseSpec(\"owner/repo@main\"))") == "['owner', 'repo', 'main']");
    CHECK(ev("String(kpm.parseSpec(\"o/r@^1.2.0\"))") == "['o', 'r', '^1.2.0']");
    CHECK(ev("String(kpm.parseSpec(\"o/r@feature/x\"))") == "['o', 'r', 'feature/x']");  // slash in ref kept
    {
        KiritoVM vm;
        vm.addLibPath(kpmDir());
        CHECK_THROWS(vm.runSource("import(\"kpm\").parseSpec(\"norepo\")\n"));  // missing '/'
    }

    // ---------- extractKpmVersion: pulls KPM_VERSION out of source text; None when absent ----------
    CHECK(ev("kpm.extractKpmVersion(\"var KPM_VERSION = \\\"9.9.9\\\"\\n\")") == "9.9.9");
    CHECK(ev("kpm.extractKpmVersion(\"no version here\")") == "None");
    // the KPM_VERSION constant is itself a valid semver (so self-update can compare against it)
    CHECK(ev("var v = import(\"semver\")\nv.valid(kpm.KPM_VERSION) != None") == "True");

    // ---------- kiAssetName: the release-asset filename for this platform ----------
    CHECK(ev("kpm.kiAssetName().startswith(\"ki-\")") == "True");
    CHECK(ev("\"x64\" in kpm.kiAssetName()") == "True");  // CI toolchains are x64

    // ---------- hasFlag ----------
    CHECK(ev("kpm.hasFlag([\"a\", \"--force\"], \"--force\")") == "True");
    CHECK(ev("kpm.hasFlag([\"a\", \"-f\"], \"--force\")") == "False");
    CHECK(ev("kpm.hasFlag([], \"--force\")") == "False");

    // ---------- ghHeaders: Accept always; Authorization only when a token is configured ----------
    CHECK(ev("kpm.ghHeaders()[\"Accept\"]") == "application/vnd.github+json");
    // explicitly clear any ambient token -> no Authorization header
    CHECK(ev("var sys = import(\"sys\")\nsys.unsetenv(\"GITHUB_TOKEN\")\n"
             "sys.unsetenv(\"KPM_GITHUB_TOKEN\")\n\"Authorization\" in kpm.ghHeaders()") == "False");
    // with a token set, the bearer header appears
    CHECK(ev("var sys = import(\"sys\")\nsys.setenv(\"KPM_GITHUB_TOKEN\", \"tok123\")\n"
             "kpm.ghHeaders()[\"Authorization\"]") == "Bearer tok123");

    return RUN_TESTS();
}
