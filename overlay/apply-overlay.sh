#!/usr/bin/env bash
#
# Apply the player-bot overlay onto a TrinityCore source tree.
#
#   apply-overlay.sh <path-to-trinitycore-src>
#
# Steps:
#   1. Copy the Custom/Bots script sources into src/server/scripts/Custom.
#   2. Apply the minimal core patches (idempotent; fails loudly on drift).
#
# CMake's CollectAndAddSourceFiles picks up everything under Custom/ automatically,
# so no CMake edits are required.
set -euo pipefail

SRC="${1:?usage: apply-overlay.sh <path-to-trinitycore-src>}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -d "${SRC}/src/server/scripts/Custom" ]]; then
    echo "ERROR: '${SRC}' does not look like a TrinityCore source tree." >&2
    exit 1
fi

echo "==> Copying Custom/Bots scripts into the source tree"
cp -vr "${HERE}/src/." "${SRC}/src/"

echo "==> Applying core patches"
python3 "${HERE}/apply_core_patches.py" "${SRC}"

echo "==> Player-bot overlay applied."
