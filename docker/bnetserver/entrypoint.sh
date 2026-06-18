#!/usr/bin/env bash
# Entrypoint for the TrinityCore bnetserver (Battle.net login server).
# Retail/master builds bnetserver instead of the legacy authserver.
set -euo pipefail
source /usr/local/lib/entrypoint-lib.sh

ETC_DIR=/opt/trinitycore/etc
BIN_DIR=/opt/trinitycore/bin
CONF="${ETC_DIR}/bnetserver.conf"
DIST="${ETC_DIR}/bnetserver.conf.dist"

# Connection / runtime settings (overridable via the environment).
DB_HOST="${TC_DB_HOST:-db}"
DB_PORT="${TC_DB_PORT:-3306}"
DB_USER="${TC_DB_USER:-trinity}"
DB_PASS="${TC_DB_PASSWORD:-trinity}"
AUTH_DB="${TC_AUTH_DB:-auth}"
LOGS_DIR="${TC_LOGS_DIR:-/opt/trinitycore/logs}"
BIND_IP="${TC_BIND_IP:-0.0.0.0}"
# Address the game client is told to reach the login REST service on.
REST_EXTERNAL="${TC_LOGIN_REST_EXTERNAL:-127.0.0.1}"
REST_LOCAL="${TC_LOGIN_REST_LOCAL:-127.0.0.1}"
REST_PORT="${TC_LOGIN_REST_PORT:-8081}"

mkdir -p "$LOGS_DIR"

# Seed config from the installed template on first run.
if [ ! -f "$CONF" ]; then
    log "Generating bnetserver.conf from template."
    cp "$DIST" "$CONF"
fi

set_conf "LoginDatabaseInfo" "$(db_info "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$AUTH_DB")" "$CONF" quote
set_conf "LogsDir" "$LOGS_DIR" "$CONF" quote
set_conf "BindIP" "$BIND_IP" "$CONF" quote
set_conf "LoginREST.ExternalAddress" "$REST_EXTERNAL" "$CONF"
set_conf "LoginREST.LocalAddress" "$REST_LOCAL" "$CONF"
set_conf "LoginREST.Port" "$REST_PORT" "$CONF"
# The bundled cert has CN "*.*", which makes bnetserver run the login service in
# plain-HTTP dev mode; modern retail clients connect over HTTPS and hang on it.
# Generate a self-signed cert with a real CN so the login REST serves HTTPS.
# (Clients reach it through a connection patcher that bypasses cert validation.)
CERT_DIR="${TC_CERT_DIR:-/opt/trinitycore/etc/certs}"
CERT="${CERT_DIR}/bnetserver.cert.pem"
KEY="${CERT_DIR}/bnetserver.key.pem"
mkdir -p "$CERT_DIR"
if [ ! -s "$CERT" ] || [ ! -s "$KEY" ]; then
    log "Generating self-signed login-REST certificate (CN=${REST_EXTERNAL})."
    openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
        -keyout "$KEY" -out "$CERT" -subj "/CN=${REST_EXTERNAL}" >/dev/null 2>&1
fi
set_conf "CertificatesFile" "$CERT" "$CONF" quote
set_conf "PrivateKeyFile" "$KEY" "$CONF" quote

wait_for_mysql "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS"
# The worldserver auto-updater creates the auth schema on first boot; bnetserver
# cannot run until the Battle.net account table exists.
wait_for_table "$DB_HOST" "$DB_PORT" "$DB_USER" "$DB_PASS" "$AUTH_DB" "battlenet_accounts"

log "Starting bnetserver."
exec "${BIN_DIR}/bnetserver" -c "$CONF"
