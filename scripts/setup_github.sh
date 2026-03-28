#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# setup_github.sh — bootstrap the dj_visualizer GitHub repository
#
# Usage: ./scripts/setup_github.sh [OPTIONS]
#   --repo    NAME    Repository name (default: auto-detected from remote or cwd)
#   --org     NAME    GitHub org      (default: your personal account)
#   --private         Make the repo private (default: public)
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail
 
# ── Helpers ───────────────────────────────────────────────────────────────
info()    { echo "  ✓ $*"; }
section() { echo ""; echo "▸ $*"; }
 
# ── Defaults — auto-detect repo name ─────────────────────────────────────
if git remote get-url origin &>/dev/null; then
  REPO_NAME=$(git remote get-url origin | sed 's|.*/||' | sed 's|\.git$||')
else
  REPO_NAME=$(basename "$(pwd)")
fi
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
 
# ── 1. Prerequisites ──────────────────────────────────────────────────────
section "Checking prerequisites"
 
if ! command -v git &>/dev/null; then
  echo "  ✗ git not found — install git first"; exit 1
fi
info "git $(git --version | awk '{print $3}')"
 
if ! command -v gh &>/dev/null; then
  echo "  ✗ GitHub CLI (gh) not found."
  echo "    Ubuntu/WSL:  sudo apt install gh"
  echo "    macOS:       brew install gh"
  exit 1
fi
info "gh $(gh --version | head -1 | awk '{print $3}')"
 
GH_USER=$(gh api user --jq '.login')
info "Logged in as: $GH_USER"
 
if [[ -n "$ORG" ]]; then
  REPO_SLUG="$ORG/$REPO_NAME"
else
  REPO_SLUG="$GH_USER/$REPO_NAME"
fi
info "Repo: github.com/$REPO_SLUG"
 
# ── 2. Create the GitHub repository ──────────────────────────────────────
section "Creating GitHub repository"
 
if gh repo view "$REPO_SLUG" &>/dev/null; then
  info "Repository already exists — skipping creation"
else
  gh repo create "$REPO_SLUG" \
    --"$VISIBILITY" \
    --description "Real-time DJ audio visualizer — C++20 · PortAudio · FFTW3 · SDL2 · OpenGL" \
    ${ORG:+--org "$ORG"}
  info "Created: github.com/$REPO_SLUG"
fi
 
# ── 3. Initialise local git ───────────────────────────────────────────────
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
 
if git remote get-url origin &>/dev/null; then
  info "Remote 'origin' already set"
else
  git remote add origin "https://github.com/$REPO_SLUG.git"
  info "Remote added: https://github.com/$REPO_SLUG.git"
fi
 
# ── 4. Push main + dev ────────────────────────────────────────────────────
section "Pushing branches"
 
git branch -M main
git push -u origin main
info "Pushed: main"
 
git checkout -b dev 2>/dev/null || git checkout dev
git push -u origin dev
info "Pushed: dev"
 
git checkout main
 
# ── 5. Branch protection — main ───────────────────────────────────────────
section "Branch protection: main"
 
gh api --method PUT "repos/$REPO_SLUG/branches/main/protection" \
  --input - << 'EOF'
{
  "required_status_checks": null,
  "enforce_admins": false,
  "required_pull_request_reviews": {
    "required_approving_review_count": 1,
    "dismiss_stale_reviews": true
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
EOF
info "main: requires PR + 1 approval, no force-push"
 
# ── 6. Branch protection — dev ────────────────────────────────────────────
section "Branch protection: dev"
 
gh api --method PUT "repos/$REPO_SLUG/branches/dev/protection" \
  --input - << 'EOF'
{
  "required_status_checks": null,
  "enforce_admins": false,
  "required_pull_request_reviews": {
    "required_approving_review_count": 1,
    "dismiss_stale_reviews": true
  },
  "restrictions": null,
  "allow_force_pushes": false,
  "allow_deletions": false
}
EOF
info "dev: requires PR + 1 approval, no force-push"
info "note: add CI check names here once workflows have run once"
 
# ── 7. Repo settings ──────────────────────────────────────────────────────
section "Repository settings"
 
gh api --method PATCH "repos/$REPO_SLUG" \
  --input - << 'EOF'
{
  "allow_squash_merge": true,
  "allow_merge_commit": false,
  "allow_rebase_merge": false,
  "squash_merge_commit_title": "PR_TITLE",
  "squash_merge_commit_message": "PR_BODY",
  "delete_branch_on_merge": true
}
EOF
info "Merge strategy: squash-only, branches auto-deleted after merge"
 
gh repo edit "$REPO_SLUG" \
  --add-topic "cpp" \
  --add-topic "audio" \
  --add-topic "opengl" \
  --add-topic "visualizer" \
  --add-topic "dj" \
  --add-topic "real-time" \
  > /dev/null
info "Topics set"
 
# ── 8. Local hooks ────────────────────────────────────────────────────────
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
