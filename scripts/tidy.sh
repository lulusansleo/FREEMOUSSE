#!/usr/bin/env bash
# Run clang-tidy against the compile_commands.json in build/.
# Requires: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON has been run.
set -e
if [[ ! -f build/compile_commands.json ]]; then
  echo "No compile_commands.json — run ./scripts/build.sh first."
  exit 1
fi
find src -name '*.cpp' | \
  xargs clang-tidy -p build
