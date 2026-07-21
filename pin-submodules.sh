#!/usr/bin/env bash
# Record the exact commit each submodule is currently at into .submodule-pins.
# Run this once after `git submodule update --init` on a known-good state,
# and again any time you deliberately move a submodule forward.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
PIN_FILE="$ROOT/.submodule-pins"

cd "$ROOT"
> "$PIN_FILE"

git submodule--helper list | while read -r _mode sha _stage path; do
    name="$(basename "$path")"
    date="$(git -C "$path" show -s --format=%ci "$sha")"
    echo "$name $sha # $date" >> "$PIN_FILE"
done

echo "Wrote $(wc -l < "$PIN_FILE") pins to $PIN_FILE"

