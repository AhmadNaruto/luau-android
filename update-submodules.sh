#!/usr/bin/env bash
# Usage: update-submodule.sh <name> <ref>
#   e.g. update-submodule.sh luau v0.650
#
# Moves ONE submodule to <ref>, reapplies its patches, and reminds you to
# rebuild/test before committing. Never touches other submodules.
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <submodule-name> <ref>" >&2
    exit 1
fi

NAME="$1"
REF="$2"
ROOT="$(git rev-parse --show-toplevel)"
SUB_PATH="$ROOT/third_party/$NAME"
PATCH_DIR="$ROOT/patches/$NAME"
PIN_FILE="$ROOT/.submodule-pins"

[[ -d "$SUB_PATH" ]] || { echo "No such submodule: $SUB_PATH" >&2; exit 1; }

echo "== Checking out $NAME @ $REF"
git -C "$SUB_PATH" fetch --tags origin
git -C "$SUB_PATH" checkout "$REF"

if [[ -d "$PATCH_DIR" ]]; then
    echo "== Reapplying patches from $PATCH_DIR"
    for patch in "$PATCH_DIR"/*.patch; do
        [[ -e "$patch" ]] || continue
        echo "   applying $(basename "$patch")"
        git -C "$SUB_PATH" apply --check "$patch" || {
            echo "!! Patch $(basename "$patch") no longer applies cleanly."
            echo "   Update the patch against $NAME@$REF before proceeding."
            exit 1
        }
        git -C "$SUB_PATH" apply "$patch"
    done
else
    echo "== No patches directory for $NAME, skipping"
fi

NEW_SHA="$(git -C "$SUB_PATH" rev-parse HEAD)"

echo ""
echo "== $NAME is now at $NEW_SHA ($REF)"
echo "== Next steps (not automated — do these before committing):"
echo "   1. Rebuild the native lib and run the $NAME-related unit tests"
echo "   2. Run ASan/UBSan pass if $NAME touches memory-sensitive paths"
echo "   3. If clean, re-run pin-submodules.sh to update $PIN_FILE"
echo "   4. Commit: submodule bump + updated pin + any patch fixups together"

