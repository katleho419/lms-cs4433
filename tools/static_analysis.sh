#!/usr/bin/env bash
# Runs the static analysers over src/. Usage: tools/static_analysis.sh [build-dir]
# clang-tidy (configuration in .clang-tidy) must report nothing; cppcheck is run when installed.
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD="${1:-build-analysis}"

cmake -S . -B "$BUILD" -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLMS_BUILD_TESTS=OFF >/dev/null

status=0
echo "== clang-tidy =="
find src -name '*.cpp' | sort | xargs clang-tidy -p "$BUILD" --quiet > "$BUILD/clang-tidy.txt" 2>&1 || true
if grep -E "warning:|error:" "$BUILD/clang-tidy.txt"; then
    status=1
else
    echo "clang-tidy: no findings"
fi

echo "== cppcheck =="
if command -v cppcheck >/dev/null; then
    cppcheck --enable=warning,style,performance,portability --std=c++17 --inline-suppr \
             --suppress=missingIncludeSystem --error-exitcode=1 -I src src 2> "$BUILD/cppcheck.txt" \
        && echo "cppcheck: no findings" || { cat "$BUILD/cppcheck.txt"; status=1; }
else
    echo "cppcheck is not installed (sudo apt install cppcheck); skipped"
fi
exit $status
