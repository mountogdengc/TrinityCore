# TrinityCore Retail Server — Setup Notes & Runbook

A record of how this server was brought up, the fixes applied to the Docker
wrapper, how to operate it, and the gotchas hit along the way.

**This is a retail / `master` server** (WoW client **12.0.5, build 67823**), run
from the local source via the Docker Compose stack at the repo root. The fork is
stock upstream TrinityCore @ `66b9c833ab` plus the Docker workflow commit — no
C++ source changes.

Set up: 2026-06-17 → 2026-06-18.

---

## Current state

| Thing | Value |
| --- | --- |
| Services | `db` (MySQL 8), `bnetserver` (login, 1119 + 8081), `worldserver` (8085) |
| Databases | `auth`, `characters`, `world`, `hotfixes` — imported (TDB1200.26021) |
| Client data | maps + vmaps + `GameObjectModels.dtree` + mmaps — complete, in volume `trinitycore_data` |
| Login | email **`david@local`**, password **`Password1`** (local throwaway) |
| Game account | `1#1`, **GM / Administrator** (security level 3) |
| Realm | "Trinity" @ `127.0.0.1:8085` |
| WSL2 memory cap | 16 GB (`C:\Users\david\.wslconfig`) |

---

## Operating the server

```powershell
docker compose ps                  # status
docker compose up -d                # start everything
docker compose stop                 # stop (keeps data)
docker compose logs -f worldserver  # tail world log
docker compose restart worldserver  # reload world (disconnects players ~1-2 min)
```

**Console / GM commands:** attach in a *real terminal* (interactive):
```powershell
docker attach tc-worldserver
# ... type commands at TC> ... then detach with Ctrl-P, Ctrl-Q  (NOT Ctrl-C)
```

**Create another account** (at the `TC>` console): `bnetaccount create <email> <password>`
(the name must be email-format). Grant GM via DB:
`INSERT INTO auth.account_access (AccountID,SecurityLevel,RealmID) VALUES (<gameAcctId>,3,-1);`

---

## Connecting a client (retail)

The retail client will not connect to a private server unmodified.

1. `Config.wtf` (in `<WoWClient>\_retail_\WTF\`): add `SET portal "127.0.0.1"`
2. Launch through the **Arctium WoW Client Launcher** (https://arctium.io/wow) —
   it patches the client to allow the custom portal and bypass cert checks.
3. Log in with the email/password above; pick the **Trinity** realm.

---

## Fixes applied to the wrapper

The wrapper originally targeted the legacy `authserver` and a 3-DB layout. The
following were needed for retail/`master`:

1. **bnetserver, not authserver** — `master` builds `bnetserver` (Battle.net
   login). Added `docker/bnetserver/entrypoint.sh`, compose service `bnetserver`
   on ports **1119** (bnet) + **8081** (login REST); `.env` `BNET_PORT` /
   `LOGIN_REST_PORT`.
2. **`libmysqlclient21`** added to the runtime image — binaries link
   `libmysqlclient.so.21`, which wasn't installed.
3. **`hotfixes` database** — retail's 4th DB. Added to the MySQL init script and
   the worldserver entrypoint (`HotfixDatabaseInfo`, updater bitmask
   `Updates.EnableDatabases=15`).
4. **`Updates.Redundancy=0`** — the TDB vs source revision skew made the updater
   try to re-apply changed update files (duplicate-column crash loop); rehash
   instead of re-apply.
5. **TDB file location + non-fatal download** — worldserver looks for the TDB
   `.sql` in its working dir (`/opt/trinitycore`); the download is now non-fatal
   (boots from the existing DB if offline).
6. **Login REST HTTPS cert** *(the big one for "can't log in / `WOW51900317`")* —
   the bundled cert has `CN=*.*`, which makes bnetserver serve the login service
   over **plain HTTP** (dev mode); modern retail clients use **HTTPS** and hang
   on it. The bnetserver entrypoint now generates a self-signed cert with
   `CN=127.0.0.1` so the login REST serves HTTPS.
7. **Entrypoints are bind-mounted** in compose, so logic changes apply on a
   restart without an image rebuild.
8. **Line endings** — the `.sh` files had CRLF in the working tree (from
   `core.autocrlf=true`); fixed to LF and set `core.autocrlf=input` locally.
9. **Map data → named volume** *(fixes in-game loading/zoning lag)* — see below.

---

## Gotchas / troubleshooting

- **`solo-wow` stack steals ports.** A separate Docker stack auto-restarts and
  binds 3306/3724/8085. Stop it when running this one:
  `docker stop solo-wow-mysql solo-wow-authserver solo-wow-worldserver`
- **"Unknown MySQL server host 'db'" after a reboot / WSL restart.** A container
  drops off the Docker network. Fix: `docker compose down && docker compose up -d`.
- **In-game loading lag = the Windows bind mount.** `/data` must NOT be a bind
  mount from a Windows drive — Docker-for-Windows' file bridge is ~100-500×
  slower for the ~259k small map/vmap/mmap files (benchmarked **44,000 ms vs
  ~400 ms** for one continent's tiles). It now lives in the external named
  volume **`trinitycore_data`** (WSL2 ext4).
  - To (re)populate the volume from a `./data` folder, do NOT extract *through* a
    bind mount (also ~3 MB/s). Instead: tar on Windows
    (`tar -cf data.tar -C data .`), `docker cp data.tar <helper>:/tmp/`, then
    `tar -xf /tmp/data.tar -C /vol` inside the helper.
  - The volume is `external` in compose, so `docker compose down -v` will NOT
    delete it. Backups remain at `E:\TrinityCore\data\` and `E:\TrinityCore\data.tar`.

---

## Still to do / optional

- Delete `E:\TrinityCore\data.tar` (~24 GB) once gameplay is confirmed — the
  `data\` folder remains as backup.
- The `XP.Boost` weekend mechanic is disabled; XP rates are blizzlike (1×).
  Adjust `Rate.XP.*` in `worldserver.conf` (via the entrypoint, to persist) if a
  different pace is wanted.
