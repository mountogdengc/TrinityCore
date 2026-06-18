#!/usr/bin/env bash
# Entrypoint for the TrinityCore authserver (realm/login server).
set -euo pipefail
source /usr/local/lib/entrypoint-lib.sh

ETC_DIR=/opt/trinitycore/etc
CONF="${ETC_DIR}/authserver.conf"
DIST="${ETC_DIR}/authserver.conf.dist"

# Connection / runtime settings (overridable via the environment).
DB_HOST="${TC_DB_HOST:-db}"
DB_PORT="${TC_DB_PORT:-3306}"
DB_USER="${TC_DB_USER:-trinity}"
DB_PASS="${TC_DB_PASSWORD:-trinity}"
AUTH_DB="${TC_AUTH_DB:-auth}"
LOGS_DIR="${TC_LOGS_DIR:-/opt/trinitycore/logs}"
BIND_IP="${TC_BIND_IP:-0.0.0.0}"

mkdir -p "$LOGS_DIR"

# Seed config from the installed template on first run.
if [ ! -f "$CONF" ]; then
    log "Generating authserver.conf from template."
    cp "$DIST" "$CONF"
fi

set_conf "LoginDatabaseInfo" "$(db_info "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$AUTH_DB")" "$CONF" quote
set_conf "LogsDir" "$LOGS_DIR" "$CONF" quote
set_conf "BindIP" "$BIND_IP" "$CONF" quote

wait_for_mysql "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS"

log "Starting authserver."
exec /opt/trinitycore/bin/authserver -c "$CONF"
