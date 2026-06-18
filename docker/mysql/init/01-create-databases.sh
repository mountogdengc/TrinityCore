#!/usr/bin/env bash
# Runs once on first MySQL initialization. Creates the three TrinityCore
# databases and a dedicated user. The schemas themselves are populated by the
# worldserver/authserver DB auto-updater on first launch.
set -euo pipefail

DB_USER="${TC_DB_USER:-trinity}"
DB_PASS="${TC_DB_PASSWORD:-trinity}"
AUTH_DB="${TC_AUTH_DB:-auth}"
CHAR_DB="${TC_CHARACTERS_DB:-characters}"
WORLD_DB="${TC_WORLD_DB:-world}"
HOTFIX_DB="${TC_HOTFIX_DB:-hotfixes}"

mysql --protocol=socket -uroot -p"${MYSQL_ROOT_PASSWORD}" <<-SQL
    CREATE DATABASE IF NOT EXISTS \`${AUTH_DB}\`   DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
    CREATE DATABASE IF NOT EXISTS \`${CHAR_DB}\`   DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
    CREATE DATABASE IF NOT EXISTS \`${WORLD_DB}\`  DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
    CREATE DATABASE IF NOT EXISTS \`${HOTFIX_DB}\` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;

    -- mysql_native_password avoids connector auth issues with MySQL 8.
    CREATE USER IF NOT EXISTS '${DB_USER}'@'%' IDENTIFIED WITH mysql_native_password BY '${DB_PASS}';

    GRANT ALL PRIVILEGES ON \`${AUTH_DB}\`.*   TO '${DB_USER}'@'%';
    GRANT ALL PRIVILEGES ON \`${CHAR_DB}\`.*   TO '${DB_USER}'@'%';
    GRANT ALL PRIVILEGES ON \`${WORLD_DB}\`.*  TO '${DB_USER}'@'%';
    GRANT ALL PRIVILEGES ON \`${HOTFIX_DB}\`.* TO '${DB_USER}'@'%';
    FLUSH PRIVILEGES;
SQL
