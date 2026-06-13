#ifndef KIRITO_VERSION_HPP
#define KIRITO_VERSION_HPP

namespace kirito {

// The Kirito release version (semantic versioning, "MAJOR.MINOR.PATCH"). Releases are git-tagged
// `vMAJOR.MINOR.PATCH`; this constant is the source of truth, surfaced as `ki --version` and the
// `sys.version` module value so Kirito programs (notably `kpm`) can read it and decide whether a
// newer interpreter is available. Bump it when cutting a release.
inline constexpr const char* kVersion = "1.1.0";

}  // namespace kirito

#endif
