#!/usr/bin/env bash
# Merge dev → main and tag a release.
# Usage: ./scripts/release.sh v0.2.0
set -e
VERSION="${1:?Usage: $0 <vX.Y.Z>}"

# Validate semver shape
if ! echo "$VERSION" | grep -qE '^v[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "Version must be vX.Y.Z — e.g. v0.2.0"; exit 1
fi

git fetch origin
git checkout main && git pull origin main
git merge --no-ff origin/dev -m "release: $VERSION"
git tag -a "$VERSION" -m "Release $VERSION"
git push origin main
git push origin "$VERSION"
echo ""
echo "Released $VERSION — CD pipeline will build the macOS .app."
