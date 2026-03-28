#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# setup_github.sh — bootstrap the dj_visualizer GitHub repository
#
# What this script does:
#   1. Checks prerequisites (git, gh CLI)
#   2. Authenticates with GitHub if needed
#   3. Creates the remote repository
#   4. Initialises local git with main + dev branches
#   5. Pushes both branches
#   6. Sets branch protection rules on main and dev
#   7. Configures the repo (description, topics, squash-merge only)
#   8. Installs local git hooks
#
# Usage: ./scripts/setup_github.sh [OPTIONS]
#   --repo    NAME    Repository name          (default: dj_visualizer)
#   --org     NAME    GitHub org (optional)    (default: your personal account)
#   --private         Make the repo private    (default: public)
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────────────────
REPO_NAME="dj_visualizer"
ORG=""
VISIBILITY="public"

# ── Parse arguments ───────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo)     REPO_NAME="$2"; shift 2 ;;
    --org)      ORG="$2";       shift 2 ;;
    --private)  VISIBILITY="private"; shift ;;
    *)          echo "Unknown option: $1"; exit 1 ;;
  esac
done

# ── Helpers ───────────────────────────────────────────────────────────────
info()    { echo "  ✓ $*"; }
section() { echo ""; echo "▸ $*"; }

# ── 1. Prerequisites ──────────────────────────────────────────────────────
section "Checking prerequisites"

if ! command -v git &>/dev/null; then
  echo "  ✗ git not found — install git first"; exit 1
fi
info "git $(git --version | awk '{print $3}')"

if ! command -v gh &>/dev/null; then
  echo ""
  echo "  ✗ GitHub CLI (gh) not found."
  echo "    Install: https://cli.github.com"
  echo "    Ubuntu/WSL:  sudo apt install gh"
  echo "    macOS:       brew install gh"
  exit 1
fi
info "gh $(gh --version | head -1 | awk '{print $3}')"

# ── 2. GitHub auth ────────────────────────────────────────────────────────
GH_USER=$(gh api user --jq '.login')
info "Using GitHub account: $GH_USER"

if [[ -n "$ORG" ]]; then
  REPO_SLUG="$ORG/$REPO_NAME"
else
  REPO_SLUG="$GH_USER/$REPO_NAME"
fi
info "Repo will be: github.com/$REPO_SLUG"

# ── 3. Create the GitHub repository ──────────────────────────────────────
section "Creating GitHub repository"

CREATE_FLAGS="--$VISIBILITY --description 'Real-time DJ audio visualizer — C++20 · PortAudio · FFTW3 · SDL2 · OpenGL'"
if [[ -n "$ORG" ]]; then
  CREATE_FLAGS="$CREATE_FLAGS --org $ORG"
fi

if gh repo view "$REPO_SLUG" &>/dev/null; then
  info "Repository already exists — skipping creation"
else
  gh repo create "$REPO_SLUG" \
    --"$VISIBILITY" \
    --description "Real-time DJ audio visualizer — C++20 · PortAudio · FFTW3 · SDL2 · OpenGL" \
    ${ORG:+--org "$ORG"}
  info "Created: github.com/$REPO_SLUG"
fi

# ── 4. Initialise local git ───────────────────────────────────────────────
section "Initialising local repository"

if [[ ! -d .git ]]; then
  git init
  info "Initialised git repository"
else
  info "Git already initialised"
fi

git add -A
git diff --cached --quiet || git commit -m "chore: initial scaffold"
info "Initial commit ready"

# Set up remote
if git remote get-url origin &>/dev/null; then
  info "Remote 'origin' already set"
else
  git remote add origin "https://github.com/$REPO_SLUG.git"
  info "Remote added: https://github.com/$REPO_SLUG.git"
fi

# ── 5. Push main + dev ────────────────────────────────────────────────────
section "Pushing branches"

# main
git branch -M main
git push -u origin main
info "Pushed: main"

# dev
git checkout -b dev 2>/dev/null || git checkout dev
git push -u origin dev
info "Pushed: dev"

git checkout main

# ── 6. Branch protection — main ───────────────────────────────────────────
section "Branch protection: main"

gh api \
  --method PUT \
  "repos/$REPO_SLUG/branches/main/protection" \
  --field required_status_checks='{"strict":true,"contexts":["CI — Linux","CI — macOS","clang-format","clang-tidy","AddressSanitizer + UBSan"]}' \
  --field enforce_admins=true \
  --field required_pull_request_reviews='{"required_approving_review_count":1,"dismiss_stale_reviews":true}' \
  --field restrictions=null \
  --field allow_force_pushes=false \
  --field allow_deletions=false \
  > /dev/null
info "main: requires PR + all CI checks green + 1 approval"

# ── 7. Branch protection — dev ────────────────────────────────────────────
section "Branch protection: dev"

gh api \
  --method PUT \
  "repos/$REPO_SLUG/branches/dev/protection" \
  --field required_status_checks='{"strict":true,"contexts":["CI — Linux","clang-format","clang-tidy"]}' \
  --field enforce_admins=false \
  --field required_pull_request_reviews='{"required_approving_review_count":1,"dismiss_stale_reviews":true}' \
  --field restrictions=null \
  --field allow_force_pushes=false \
  --field allow_deletions=false \
  > /dev/null
info "dev: requires PR + Linux CI + format/tidy green + 1 approval"

# ── 8. Repo settings ──────────────────────────────────────────────────────
section "Repository settings"

gh api \
  --method PATCH \
  "repos/$REPO_SLUG" \
  --field allow_squash_merge=true \
  --field allow_merge_commit=false \
  --field allow_rebase_merge=false \
  --field squash_merge_commit_title="PR_TITLE" \
  --field squash_merge_commit_message="PR_BODY" \
  --field delete_branch_on_merge=true \
  > /dev/null
info "Merge strategy: squash-only (keeps history clean)"
info "Branches auto-deleted after merge"

gh repo edit "$REPO_SLUG" \
  --add-topic "cpp" \
  --add-topic "audio" \
  --add-topic "opengl" \
  --add-topic "visualizer" \
  --add-topic "dj" \
  --add-topic "real-time" \
  > /dev/null
info "Topics set"

# ── 9. Local hooks ────────────────────────────────────────────────────────
section "Installing local git hooks"
git config core.hooksPath .githooks
info "Hooks active: pre-commit (format), commit-msg (conventional commits)"

# ── Done ──────────────────────────────────────────────────────────────────
echo ""
echo "┌─────────────────────────────────────────────────────────┐"
echo "│  All done! Your repo is live.                           │"
echo "│                                                         │"
printf "│  %-55s│\n" "  https://github.com/$REPO_SLUG"
echo "│                                                         │"
echo "│  Day-to-day workflow:                                   │"
echo "│    ./scripts/new_feature.sh feat/my-feature            │"
echo "│    # ... develop ...                                    │"
echo "│    git add -p && git commit -m 'feat(render): ...'      │"
echo "│    git push && open a PR → dev on GitHub                │"
echo "│                                                         │"
echo "│  To ship a release:                                     │"
echo "│    ./scripts/release.sh v0.1.0                         │"
echo "│    # CD pipeline builds the macOS .app automatically   │"
echo "└─────────────────────────────────────────────────────────┘"
