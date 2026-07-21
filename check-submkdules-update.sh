#!/usr/bin/env bash
# Report available upstream updates for each submodule, compared against
# the committed pin in .submodule-pins. Does NOT change anything.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
PIN_FILE="$ROOT/.submodule-pins"

if [[ ! -f "$PIN_FILE" ]]; then
    echo "No $PIN_FILE found — run pin-submodules.sh first." >&2
    exit 1
fi

cd "$ROOT"

while read -r name pinned_sha _rest; do
    [[ -z "$name" ]] && continue
    path=$(git config -f .gitmodules --get-regexp path | grep "/$name\$" | awk '{print $2}')
    [[ -z "$path" ]] && { echo "warn: no path for $name in .gitmodules" >&2; continue; }

    remote=$(git config -f .gitmodules "submodule.$path.branch" 2>/dev/null || echo "HEAD")
    latest_sha=$(git -C "$path" ls-remote origin "refs/heads/${remote}" 2>/dev/null | awk '{print $1}')
    [[ -z "$latest_sha" ]] && latest_sha=$(git -C "$path" ls-remote origin HEAD | awk '{print $1}')

    if [[ "$latest_sha" == "$pinned_sha" ]]; then
        echo "== $name: up to date ($pinned_sha)"
    else
        behind=$(git -C "$path" rev-list --count "$pinned_sha..$latest_sha" 2>/dev/null || echo "?")
        echo "!! $name: pinned=$pinned_sha latest=$latest_sha ($behind commits behind)"
    fi
done < "$PIN_FILE"

