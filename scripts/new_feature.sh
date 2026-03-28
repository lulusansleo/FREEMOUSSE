#!/usr/bin/env bash
# Create and push a new feature branch off dev.
# Usage: ./scripts/new_feature.sh feat/my-cool-mode
#        ./scripts/new_feature.sh fix/beat-crash
set -e
BRANCH="${1:?Usage: $0 <feat/name | fix/name | perf/name>}"

git fetch origin
git checkout dev
git pull origin dev
git checkout -b "$BRANCH"
git push -u origin "$BRANCH"
echo ""
echo "Branch '$BRANCH' created and pushed."
echo "When done: open a PR targeting 'dev' on GitHub."
