#!/usr/bin/env bash
##############################################################################
# Extract TrinityCore client data (dbc/db2, maps, vmaps, mmaps, cameras) from a
# retail WoW client using the tools built into the trinitycore image.
#
# Run via Compose (recommended):
#   WOW_CLIENT_DIR=/path/to/WoW docker compose run --rm extractor
#
# This follows the canonical TrinityCore extraction sequence. The tools write
# their output into the current working directory (the mounted client folder),
# then this script moves the results into OUTPUT_DIR.
#
# NOTE: A legally-obtained, matching retail WoW client is required. Client
# files cannot be redistributed and are never shipped in this repo.
##############################################################################
set -euo pipefail

BIN=/opt/trinitycore/bin
CLIENT_DIR="${PWD}"
OUTPUT_DIR="${OUTPUT_DIR:-/out}"
JOBS="${EXTRACT_JOBS:-$(nproc)}"

echo "[extract] Client : ${CLIENT_DIR}"
echo "[extract] Output : ${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

if [ ! -d "${CLIENT_DIR}/Data" ]; then
    echo "[extract] ERROR: no 'Data' folder found in ${CLIENT_DIR}." >&2
    echo "[extract] Mount your WoW client at /wow (set WOW_CLIENT_DIR)." >&2
    exit 1
fi

echo "[extract] 1/4 mapextractor (dbc/db2, maps, cameras, gt)..."
"${BIN}/mapextractor"

echo "[extract] 2/4 vmap4extractor (raw vmaps -> Buildings/)..."
"${BIN}/vmap4extractor"

echo "[extract] 3/4 vmap4assembler (Buildings -> vmaps/)..."
"${BIN}/vmap4assembler" Buildings vmaps

echo "[extract] 4/4 mmaps_generator (navigation meshes -> mmaps/)..."
"${BIN}/mmaps_generator" --threads "${JOBS}"

echo "[extract] Moving extracted data into ${OUTPUT_DIR} ..."
for d in dbc db2 maps vmaps mmaps Cameras cameras gt; do
    if [ -d "${CLIENT_DIR}/${d}" ]; then
        rm -rf "${OUTPUT_DIR:?}/${d}"
        mv "${CLIENT_DIR}/${d}" "${OUTPUT_DIR}/"
        echo "[extract]   moved ${d}/"
    fi
done

# Buildings/ is an intermediate artifact; clean it up.
rm -rf "${CLIENT_DIR}/Buildings"

echo "[extract] Done. DataDir contents:"
ls -1 "${OUTPUT_DIR}"
