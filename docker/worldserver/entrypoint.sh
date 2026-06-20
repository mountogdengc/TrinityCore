#!/usr/bin/env bash
# Entrypoint for the TrinityCore worldserver (game world server).
set -euo pipefail
source /usr/local/lib/entrypoint-lib.sh

ETC_DIR=/opt/trinitycore/etc
CONF="${ETC_DIR}/worldserver.conf"
DIST="${ETC_DIR}/worldserver.conf.dist"
# The worldserver looks for the TDB full-import .sql in its working directory
# (/opt/trinitycore), not the compiled-in source sql tree.
SQL_DIR="${TC_TDB_DIR:-/opt/trinitycore}"

# Connection / runtime settings (overridable via the environment).
DB_HOST="${TC_DB_HOST:-db}"
DB_PORT="${TC_DB_PORT:-3306}"
DB_USER="${TC_DB_USER:-trinity}"
DB_PASS="${TC_DB_PASSWORD:-trinity}"
AUTH_DB="${TC_AUTH_DB:-auth}"
CHAR_DB="${TC_CHARACTERS_DB:-characters}"
WORLD_DB="${TC_WORLD_DB:-world}"
HOTFIX_DB="${TC_HOTFIX_DB:-hotfixes}"
DATA_DIR="${TC_DATA_DIR:-/data}"
LOGS_DIR="${TC_LOGS_DIR:-/opt/trinitycore/logs}"
BIND_IP="${TC_BIND_IP:-0.0.0.0}"
# DB auto-updater bitmask: 1=auth 2=characters 4=world 8=hotfixes -> 15=all.
# Retail/master adds the hotfixes database; legacy branches use 7.
UPDATES_ENABLE="${TC_UPDATES_ENABLE_DATABASES:-15}"
UPDATES_AUTOSETUP="${TC_UPDATES_AUTOSETUP:-1}"
# Redundancy=0: when a previously-applied update file's hash changed, rehash it
# instead of re-applying (with AllowRehash=1). A fork's locally-modified SQL
# tree otherwise triggers re-apply failures (e.g. duplicate-column errors).
UPDATES_REDUNDANCY="${TC_UPDATES_REDUNDANCY:-0}"
# SOAP remote console (used by ./tc account and ./tc cmd). Off unless enabled.
SOAP_ENABLED="${TC_SOAP_ENABLED:-0}"
SOAP_IP="${TC_SOAP_IP:-0.0.0.0}"
SOAP_PORT="${TC_SOAP_PORT:-7878}"
# Player-bots: a bot that follows its (often GM) master into a dungeon is a real,
# possibly low-level character and must pass the instance LEVEL gate the GM master
# bypasses (MapDifficultyConditions + access_requirement, see Player::Satisfy).
# Default on so bots of any level can zone in; set 0 to restore retail behaviour.
INSTANCE_IGNORE_LEVEL="${TC_INSTANCE_IGNORE_LEVEL:-1}"
# Custom: Death QoL (see docs/superpowers/specs/2026-06-20-death-qol-design.md).
# All default OFF (vanilla); flip to 1 to opt in. Player auto-revive teleports a
# dead player back to their corpse so there's no graveyard run; bot auto-revive
# resurrects a dead bot at its master while the master is alive.
ALLOW_CHAT_WHILE_DEAD="${TC_ALLOW_CHAT_WHILE_DEAD:-0}"
PLAYER_AUTO_REVIVE_AT_CORPSE="${TC_PLAYER_AUTO_REVIVE_AT_CORPSE:-0}"
PLAYER_AUTO_REVIVE_DELAY_MS="${TC_PLAYER_AUTO_REVIVE_DELAY_MS:-5000}"
BOT_AUTO_REVIVE="${TC_BOT_AUTO_REVIVE:-0}"
BOT_AUTO_REVIVE_DELAY_MS="${TC_BOT_AUTO_REVIVE_DELAY_MS:-5000}"
# Account-wide collection grants (toys/heirlooms/appearances/warband scenes) default
# ON in the core, but granting ALL appearances on every login builds an enormous
# collection update that crashes a real client's login. Off by default here until
# the grant is batched/safe; set per-flag to 1 to opt back in.
COLLECTIONS_GRANT_ALL_TOYS="${TC_COLLECTIONS_GRANT_ALL_TOYS:-0}"
COLLECTIONS_GRANT_ALL_HEIRLOOMS="${TC_COLLECTIONS_GRANT_ALL_HEIRLOOMS:-0}"
COLLECTIONS_GRANT_ALL_APPEARANCES="${TC_COLLECTIONS_GRANT_ALL_APPEARANCES:-0}"
COLLECTIONS_GRANT_ALL_WARBAND_SCENES="${TC_COLLECTIONS_GRANT_ALL_WARBAND_SCENES:-0}"

mkdir -p "$LOGS_DIR" "$DATA_DIR"

# Optionally download + extract the TDB (full world DB) so the auto-updater can
# import it into an empty `world` database on first launch.
if [ -n "${TC_TDB_URL:-}" ]; then
    if ! ls "${SQL_DIR}"/TDB_full_world_*.sql >/dev/null 2>&1 \
       || ! ls "${SQL_DIR}"/TDB_full_hotfixes_*.sql >/dev/null 2>&1; then
        log "Downloading TDB from ${TC_TDB_URL}"
        tmp=$(mktemp -d)
        # Non-fatal: if the download fails (e.g. no network/DNS) but the
        # databases are already populated, the worldserver still boots fine.
        if curl -fSL "$TC_TDB_URL" -o "${tmp}/tdb.7z"; then
            7z e -y -o"${tmp}" "${tmp}/tdb.7z" >/dev/null
            cp "${tmp}"/TDB_full_world_*.sql "${SQL_DIR}/" 2>/dev/null || true
            cp "${tmp}"/TDB_full_hotfixes_*.sql "${SQL_DIR}/" 2>/dev/null || true
            log "TDB (world + hotfixes) placed in ${SQL_DIR}."
        else
            log "WARNING: TDB download failed (network/DNS). Continuing with existing databases."
        fi
        rm -rf "$tmp"
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
set_conf "HotfixDatabaseInfo"    "$(db_info "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$HOTFIX_DB")" "$CONF" quote
set_conf "DataDir"                 "$DATA_DIR"          "$CONF" quote
set_conf "LogsDir"                 "$LOGS_DIR"          "$CONF" quote
set_conf "BindIP"                  "$BIND_IP"           "$CONF" quote
set_conf "Updates.EnableDatabases" "$UPDATES_ENABLE"     "$CONF"
set_conf "Updates.AutoSetup"       "$UPDATES_AUTOSETUP"  "$CONF"
set_conf "Updates.Redundancy"      "$UPDATES_REDUNDANCY" "$CONF"
set_conf "SOAP.Enabled"            "$SOAP_ENABLED"       "$CONF"
set_conf "SOAP.IP"                 "$SOAP_IP"            "$CONF" quote
set_conf "SOAP.Port"               "$SOAP_PORT"          "$CONF"
set_conf "Instance.IgnoreLevel"    "$INSTANCE_IGNORE_LEVEL" "$CONF"
set_conf "Custom.AllowChatWhileDead"        "$ALLOW_CHAT_WHILE_DEAD"        "$CONF"
set_conf "Custom.PlayerAutoReviveAtCorpse"  "$PLAYER_AUTO_REVIVE_AT_CORPSE" "$CONF"
set_conf "Custom.PlayerAutoReviveDelayMs"   "$PLAYER_AUTO_REVIVE_DELAY_MS"  "$CONF"
set_conf "Custom.BotAutoRevive"             "$BOT_AUTO_REVIVE"              "$CONF"
set_conf "Custom.BotAutoReviveDelayMs"      "$BOT_AUTO_REVIVE_DELAY_MS"     "$CONF"
set_conf "Collections.GrantAllToys"          "$COLLECTIONS_GRANT_ALL_TOYS"          "$CONF"
set_conf "Collections.GrantAllHeirlooms"     "$COLLECTIONS_GRANT_ALL_HEIRLOOMS"     "$CONF"
set_conf "Collections.GrantAllAppearances"   "$COLLECTIONS_GRANT_ALL_APPEARANCES"   "$CONF"
set_conf "Collections.GrantAllWarbandScenes" "$COLLECTIONS_GRANT_ALL_WARBAND_SCENES" "$CONF"

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
