#ifndef KIRITO_VERSION_HPP
#define KIRITO_VERSION_HPP

namespace kirito {

// The Kirito release version (semantic versioning, "MAJOR.MINOR.PATCH"). Releases are git-tagged
// with the BARE version (e.g. `1.6.0`; the release.yml CI workflow fires on those tags and a
// leading `v` is also tolerated). This constant is the source of truth, surfaced as `ki --version`
// and the `sys.version` module value so Kirito programs (notably `kpm`) can read it and decide
// whether a newer interpreter is available. Bump it IN THE SAME COMMIT you tag, so the published
// binary's embedded version matches its release (CI verifies this and refuses to ship a mismatch).
inline constexpr const char* kVersion = "1.6.1";

}  // namespace kirito

#endif
