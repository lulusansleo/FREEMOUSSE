#!/usr/bin/env bash
# Run clang-format in-place over all C++ sources.
# Usage: ./scripts/format.sh          — format everything
#        ./scripts/format.sh --check  — dry-run (exits 1 if any diff)
set -e
MODE=""
if [[ "$1" == "--check" ]]; then MODE="--dry-run --Werror"; fi
find src include -name '*.cpp' -o -name '*.hpp' | \
  xargs clang-format $MODE
echo "format: done"
