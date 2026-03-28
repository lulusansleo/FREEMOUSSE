#!/usr/bin/env bash
# Point Git at .githooks/ so hooks are version-controlled.
git config core.hooksPath .githooks
echo "Git hooks installed from .githooks/"
