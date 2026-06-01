#!/usr/bin/env bash
# Build shippable RELEASE binaries of the `ki` interpreter into dist/.
#
# Each binary is an optimized Release build with TLS (HTTPS in the `net` module) enabled and linked
# **as statically as possible**: OpenSSL, libstdc++ and libgcc are pulled in statically. On Linux
# only glibc stays dynamic — a fully `-static` glibc breaks DNS / NSS name resolution, which the net
# module needs, so this is the maximally-static build that still actually resolves hosts.
#
# This script builds every target whose toolchain is present in the current environment and SKIPs
# the rest (a Linux box without mingw can't produce the Windows binaries, etc.). The full matrix —
# Windows + Linux, 32- and 64-bit — is produced on CI by .github/workflows/release.yml, which runs
# on the appropriate native runners and publishes the binaries to a GitHub Release.
#
# Usage:  scripts/build_release.sh

set -u
cd "$(dirname "$0")/.."
mkdir -p dist
have() { command -v "$1" >/dev/null 2>&1; }
rc=0

# build  <output-filename>  <build-dir>  <cmake-args...>
build() {
    local out="$1" dir="$2"; shift 2
    echo "=================================================================="
    echo "  $out"
    echo "=================================================================="
    rm -rf "$dir"
    if cmake -S . -B "$dir" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DKIRITO_ENABLE_TLS=ON -DOPENSSL_USE_STATIC_LIBS=ON \
            "$@" > "$dir.log" 2>&1 \
       && cmake --build "$dir" --target ki >> "$dir.log" 2>&1; then
        cp "$dir/ki" "dist/$out" 2>/dev/null || cp "$dir/ki.exe" "dist/$out" 2>/dev/null
        echo "OK -> dist/$out"
    else
        echo "FAIL[$out] — see $dir.log"; tail -3 "$dir.log"; rc=1
    fi
}

# --- Linux x86_64 (host) ---
if [ "$(uname -s)" = "Linux" ]; then
    build "ki-linux-x64" "dist/build-linux-x64" -DCMAKE_EXE_LINKER_FLAGS="-s"

    # --- Linux i686 (needs gcc-multilib + libssl-dev:i386) ---
    if echo 'int main(){}' | g++ -m32 -x c++ - -o /dev/null >/dev/null 2>&1; then
        build "ki-linux-x86" "dist/build-linux-x86" \
            -DCMAKE_CXX_FLAGS="-O2 -m32" -DCMAKE_EXE_LINKER_FLAGS="-s -m32"
    else
        echo "SKIP[ki-linux-x86]: no working 32-bit toolchain (need gcc-multilib + libssl-dev:i386)"
    fi
fi

# --- Windows via mingw-w64 cross compilers (if installed) ---
for arch in x64 x86; do
    cxx="x86_64-w64-mingw32-g++"; [ "$arch" = x86 ] && cxx="i686-w64-mingw32-g++"
    if have "$cxx"; then
        build "ki-windows-$arch.exe" "dist/build-win-$arch" \
            -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_CXX_COMPILER="$cxx" \
            -DCMAKE_EXE_LINKER_FLAGS="-s -static"
    else
        echo "SKIP[ki-windows-$arch]: $cxx not found (use CI for Windows binaries)"
    fi
done

echo "=================================================================="
ls -la dist/ | grep -E 'ki-' || echo "(no binaries produced)"
exit "$rc"
