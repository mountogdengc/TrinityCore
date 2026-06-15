#!/usr/bin/env bash
# Shared helpers for the authserver / worldserver container entrypoints.
set -euo pipefail

log() { printf '[entrypoint] %s\n' "$*"; }

# set_conf <key> <value> <file> [quote]
# Sets "key = value" in a TrinityCore .conf file, replacing an existing
# (possibly commented) line or appending if the key is absent.
# Pass "quote" as the 4th arg to wrap string values in double quotes.
set_conf() {
    local key="$1" value="$2" file="$3" quote="${4:-}"
    local rendered
    if [ "$quote" = "quote" ]; then
        rendered="${key} = \"${value}\""
    else
        rendered="${key} = ${value}"
    fi

    if grep -qE "^[#[:space:]]*${key}[[:space:]]*=" "$file"; then
        # Escape characters that are special to sed's replacement.
        local esc
        esc=$(printf '%s' "$rendered" | sed -e 's/[&|\\]/\\&/g')
        sed -i -E "s|^[#[:space:]]*${key}[[:space:]]*=.*|${esc}|" "$file"
    else
        printf '%s\n' "$rendered" >> "$file"
    fi
}

# Build a TrinityCore DB connection string: host;port;user;pass;db
db_info() {
    printf '%s;%s;%s;%s;%s' "$1" "$2" "$3" "$4" "$5"
}

# wait_for_mysql <host> <port> <user> <pass>
wait_for_mysql() {
    local host="$1" port="$2" user="$3" pass="$4"
    log "Waiting for MySQL at ${host}:${port} ..."
    until mysqladmin ping -h "$host" -P "$port" -u "$user" -p"$pass" --silent >/dev/null 2>&1; do
        sleep 2
    done
    log "MySQL is up."
}

# wait_for_tcp <host> <port> [label]
wait_for_tcp() {
    local host="$1" port="$2" label="${3:-$host:$port}"
    log "Waiting for ${label} ..."
    until nc -z "$host" "$port" >/dev/null 2>&1; do
        sleep 2
    done
    log "${label} is reachable."
}
