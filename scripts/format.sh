#!/usr/bin/env bash
# Run clang-format in-place over all C++ sources.
# Usage: ./scripts/format.sh          — format everything
#        ./scripts/format.sh --check  — dry-run (exits 1 if any diff)
set -e
MODE=(-i)
if [[ "$1" == "--check" ]]; then MODE=(--dry-run --Werror); fi

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is not installed. Run: ./scripts/install_deps.sh"
  exit 127
fi

find src include \( -name '*.cpp' -o -name '*.hpp' \) -print0 | \
  xargs -0 clang-format "${MODE[@]}"
echo "format: done"
