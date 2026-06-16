#!/usr/bin/env bash
# Entrypoint for the TrinityCore worldserver (game world server).
set -euo pipefail
source /usr/local/lib/entrypoint-lib.sh

ETC_DIR=/opt/trinitycore/etc
CONF="${ETC_DIR}/worldserver.conf"
DIST="${ETC_DIR}/worldserver.conf.dist"
SQL_DIR=/opt/trinitycore-src/sql

# Connection / runtime settings (overridable via the environment).
DB_HOST="${TC_DB_HOST:-db}"
DB_PORT="${TC_DB_PORT:-3306}"
DB_USER="${TC_DB_USER:-trinity}"
DB_PASS="${TC_DB_PASSWORD:-trinity}"
AUTH_DB="${TC_AUTH_DB:-auth}"
CHAR_DB="${TC_CHARACTERS_DB:-characters}"
WORLD_DB="${TC_WORLD_DB:-world}"
DATA_DIR="${TC_DATA_DIR:-/data}"
LOGS_DIR="${TC_LOGS_DIR:-/opt/trinitycore/logs}"
BIND_IP="${TC_BIND_IP:-0.0.0.0}"
# DB auto-updater bitmask: 1=auth 2=characters 4=world -> 7=all.
UPDATES_ENABLE="${TC_UPDATES_ENABLE_DATABASES:-7}"
UPDATES_AUTOSETUP="${TC_UPDATES_AUTOSETUP:-1}"

mkdir -p "$LOGS_DIR" "$DATA_DIR"

# Optionally download + extract the TDB (full world DB) so the auto-updater can
# import it into an empty `world` database on first launch.
if [ -n "${TC_TDB_URL:-}" ]; then
    if ! ls "${SQL_DIR}"/TDB_full_world_*.sql >/dev/null 2>&1; then
        log "Downloading TDB from ${TC_TDB_URL}"
        tmp=$(mktemp -d)
        curl -fSL "$TC_TDB_URL" -o "${tmp}/tdb.7z"
        7z e -y -o"${tmp}" "${tmp}/tdb.7z" >/dev/null
        cp "${tmp}"/TDB_full_world_*.sql "${SQL_DIR}/"
        rm -rf "$tmp"
        log "TDB placed in ${SQL_DIR}."
    else
        log "TDB already present in ${SQL_DIR}, skipping download."
    fi
fi

# Seed config from the installed template on first run.
if [ ! -f "$CONF" ]; then
    log "Generating worldserver.conf from template."
    cp "$DIST" "$CONF"
fi

set_conf "LoginDatabaseInfo"     "$(db_info "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$AUTH_DB")"  "$CONF" quote
set_conf "WorldDatabaseInfo"     "$(db_info "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$WORLD_DB")" "$CONF" quote
set_conf "CharacterDatabaseInfo" "$(db_info "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$CHAR_DB")"  "$CONF" quote
set_conf "DataDir"                 "$DATA_DIR"          "$CONF" quote
set_conf "LogsDir"                 "$LOGS_DIR"          "$CONF" quote
set_conf "BindIP"                  "$BIND_IP"           "$CONF" quote
set_conf "Updates.EnableDatabases" "$UPDATES_ENABLE"    "$CONF"
set_conf "Updates.AutoSetup"       "$UPDATES_AUTOSETUP" "$CONF"

# Warn loudly if client data is missing -- the worldserver cannot start without
# extracted maps. See scripts/extract-client-data.sh.
if [ ! -d "${DATA_DIR}/maps" ]; then
    log "WARNING: ${DATA_DIR}/maps not found. The worldserver needs extracted"
    log "         client data (dbc, maps, vmaps, mmaps, cameras) in ${DATA_DIR}."
    log "         Run scripts/extract-client-data.sh against your WoW client first."
fi

wait_for_mysql "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS"
# The authserver applies the auth DB updates; give it a head start if present.
if [ -n "${TC_WAIT_FOR_AUTH:-}" ]; then
    wait_for_tcp "${TC_AUTH_HOST:-authserver}" "${TC_AUTH_PORT:-3724}" "authserver"
fi

log "Starting worldserver."
exec /opt/trinitycore/bin/worldserver -c "$CONF"
