#!/usr/bin/env bash
set -euo pipefail

UPSTREAM_SLUG="sigrokproject/libsigrok"       # original
FORK_SLUG="OpenTraceLab/OpenTraceCapture"     # your fork

# Detect default branch of your fork (main/master)
DEFAULT_BRANCH="$(git symbolic-ref --quiet --short refs/remotes/origin/HEAD | sed 's|^origin/||' || true)"
if [[ -z "${DEFAULT_BRANCH}" ]]; then
  DEFAULT_BRANCH="$(gh repo view "${FORK_SLUG}" --json defaultBranchRef -q .defaultBranchRef.name)"
fi
echo "Using default branch: ${DEFAULT_BRANCH}"

git fetch origin --prune
git fetch upstream --prune

# Get OPEN PRs and sort by number (oldest first)
mapfile -t PRS < <(gh pr list -R "${UPSTREAM_SLUG}" --state open --limit 1000 --json number,createdAt -q '.[] | [.createdAt, .number] | @tsv' | sort | cut -f2)

# Labels (idempotent)
gh label create upstream-port --color 0e8a16 --description "Ported from upstream PR" 2>/dev/null || true
gh label create needs-manual-rebase --color d93f0b --description "Automated rebase failed" 2>/dev/null || true

for PR in "${PRS[@]}"; do
  SRC="upstream-pr/${PR}"      # local mirror of upstream PR head
  PORT="port/pr-${PR}"         # branch in your fork that rebases onto your default

  echo -e "\n=== PR #${PR} ==="

  # Fetch the upstream PR head (preserves original commits & authors)
  git fetch "https://github.com/${UPSTREAM_SLUG}.git" "pull/${PR}/head:${SRC}" || { echo "skip ${PR}"; continue; }

  # Start from your default branch and try a clean rebase of the PR commits
  git checkout -B "${PORT}" "${DEFAULT_BRANCH}"
  if git rebase "${SRC}"; then
    # Success: push and open/update a PR in your fork
    git push -u origin "${PORT}" || true

    TITLE="$(gh pr view -R "${UPSTREAM_SLUG}" "${PR}" --json title -q .title 2>/dev/null || echo "Upstream PR #${PR}")"
    BODY="$(gh pr view -R "${UPSTREAM_SLUG}" "${PR}" --json body -q .body 2>/dev/null || echo "")"

    EXISTING="$(gh pr list -R "${FORK_SLUG}" --head "${PORT}" --json number -q '.[0].number' || true)"
    if [[ -z "${EXISTING}" ]]; then
      gh pr create -R "${FORK_SLUG}" \
        --head "${PORT}" --base "${DEFAULT_BRANCH}" \
        --title "Upstream PR #${PR}: ${TITLE}" \
        --body "Ports upstream PR #${PR} from \`${UPSTREAM_SLUG}\`.\n\nOriginal: https://github.com/${UPSTREAM_SLUG}/pull/${PR}\n\n---\nOriginal body:\n\n${BODY}" \
        --label upstream-port
    else
      gh pr edit "${EXISTING}" -R "${FORK_SLUG}" --add-label upstream-port || true
    fi
  else
    # Conflict: back out and open a PR marked for manual fix
    git rebase --abort || true
    git checkout -B "${PORT}" "${DEFAULT_BRANCH}"
    git merge --no-ff "${SRC}" || true   # create a conflict snapshot you can resolve
    git push -u origin "${PORT}" || true

    EXISTING="$(gh pr list -R "${FORK_SLUG}" --head "${PORT}" --json number -q '.[0].number' || true)"
    if [[ -z "${EXISTING}" ]]; then
      gh pr create -R "${FORK_SLUG}" \
        --head "${PORT}" --base "${DEFAULT_BRANCH}" \
        --title "⚠️ Upstream PR #${PR}: manual rebase needed" \
        --body "Automated rebase failed for upstream PR #${PR}.\nOriginal: https://github.com/${UPSTREAM_SLUG}/pull/${PR}\n\nResolve conflicts on \`${PORT}\` and push." \
        --label upstream-port --label needs-manual-rebase
    else
      gh pr edit "${EXISTING}" -R "${FORK_SLUG}" --add-label needs-manual-rebase || true
    fi
  fi
done

echo -e "\nDone. Review PRs in ${FORK_SLUG}."
