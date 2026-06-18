# TrinityCore Emulator — Docker Compose Deployment

A reproducible [Docker Compose](https://docs.docker.com/compose/) stack that
builds and runs a [TrinityCore](https://www.trinitycore.org/) World of Warcraft
server (the **`master`** branch, which targets the **current retail client** —
e.g. **12.0.5, build 67823**).

The stack runs three services:

| Service       | Role                                   | Port  |
|---------------|----------------------------------------|-------|
| `db`          | MySQL 8.0 (auth/characters/world DBs)  | 3306  |
| `authserver`  | Login / realm server                   | 3724  |
| `worldserver` | Game world server (+ console)          | 8085  |

A multi-stage image compiles `worldserver`, `authserver` and the data-extraction
tools from source. The MySQL databases are populated automatically on first
launch by TrinityCore's built-in database auto-updater.

---

## ⚠️ What you must provide yourself

Two things cannot be shipped in this repo and are required to run a *playable*
server:

1. **A retail WoW client** matching the `master` branch (you have **12.0.5,
   build 67823** — good). Blizzard client files are copyrighted and never
   redistributed here.
2. **Extracted client data** (dbc/db2, maps, vmaps, mmaps, cameras), generated
   from *your* client with the bundled extractor tools. The worldserver will not
   start without it.

Everything else (build, DB schema import, config) is automated.

---

## Prerequisites

- Docker Engine + the Docker Compose plugin (`docker compose`).
- ~**8 GB RAM** recommended for the build, and significant disk (the source
  build + extracted retail data is large; budget tens of GB).
- A good internet connection (the build clones TrinityCore and downloads the
  TDB world database).

---

## Quick start

The `./tc` helper wraps the common Docker Compose operations. Run `./tc help`
for the full list.

```bash
# 1. Configure
./tc init                 # copies .env.example -> .env
#    Edit .env:
#      - set TC_TDB_URL to the TDB release matching `master` (see below)
#      - set WOW_CLIENT_DIR to your WoW client folder
#      - change the default passwords

# 2. Build the image (first build takes a while)
./tc build

# 3. Extract client data from your WoW client (build 67823)
WOW_CLIENT_DIR=/path/to/WoW ./tc extract
#    -> writes dbc/db2, maps, vmaps, mmaps, cameras into ./data

# 4. Start the full stack (worldserver imports the DBs on first run)
./tc up

# 5. Watch the world server come up and import the database
./tc logs
```

First boot of `worldserver` will create the schema, import the TDB, and apply
updates — this can take several minutes. Subsequent starts are fast.

> Prefer raw Compose? Every `./tc` command maps to a `docker compose` call; see
> the table in [The `tc` helper](#the-tc-helper) below.

---

## Choosing the right TDB (world database)

The full world database (creatures, quests, gameobjects, …) ships as a **TDB**
release that must match your branch. Pick the asset from the
[TrinityCore releases page](https://github.com/TrinityCore/TrinityCore/releases)
and set it in `.env`:

```env
TC_TDB_URL=https://github.com/TrinityCore/TrinityCore/releases/download/TDB<ver>/TDB_full_world_<ver>_<date>.7z
```

On first launch the worldserver downloads the `.7z`, extracts the `.sql` into
the source `sql/` tree, and the auto-updater imports it into the empty `world`
database. If you leave `TC_TDB_URL` empty, you must import a world database
yourself.

---

## Extracting client data

The `extractor` service (Compose profile `tools`) runs the official extraction
sequence against a mounted client:

```bash
WOW_CLIENT_DIR=/path/to/WoW docker compose run --rm extractor
```

This runs, in order: `mapextractor` → `vmap4extractor` →
`vmap4assembler` → `mmaps_generator`, and moves the results into `./data`
(mounted at `/data` in the worldserver as `DataDir`). `mmaps_generator` is
CPU-intensive and can take a long time for a full extraction.

> The tools write intermediate output into the client folder, so mount a client
> you can write to. See `scripts/extract-client-data.sh`.

---

## The `tc` helper

`./tc <command>` is a thin wrapper around the stack:

| Command | What it does |
|---|---|
| `./tc init` | Create `.env` from the template |
| `./tc build` | Build the TrinityCore image |
| `./tc extract` | Extract client data into `./data` |
| `./tc up` | Start db + authserver + worldserver |
| `./tc down` | Stop (keeps the DB volume) |
| `./tc destroy` | Stop and **delete** the DB volume |
| `./tc restart [svc]` | Restart all, or one service |
| `./tc ps` | Service status |
| `./tc logs [svc]` | Follow logs (default: worldserver) |
| `./tc console` | Attach to the worldserver console |
| `./tc account <name> <pass> [gm]` | Create an account via SOAP |
| `./tc cmd "<command>"` | Run a console command via SOAP |
| `./tc sql` | MySQL shell as the trinity user |
| `./tc shell [svc]` | Bash shell in a container |

## Creating a game account

**Your first account must be created from the interactive console** (SOAP needs
an existing GM account to authenticate):

```bash
./tc console
# at the TC> prompt:
account create <username> <password>
account set gmlevel <username> 3 -1
# detach WITHOUT stopping the server: press Ctrl-p, then Ctrl-q
```

After that first GM account exists, set `TC_SOAP_USER`/`TC_SOAP_PASSWORD` in
`.env` to its credentials and you can script the rest over SOAP:

```bash
./tc account alice s3cret 3      # create + set gm level 3
./tc cmd "server info"           # any console command
```

Point your client's `realmlist.wtf` (or equivalent for retail builds) at the
host running `authserver`. By default the `realmlist` table in the `auth`
database points realms at `127.0.0.1`; update it for LAN/remote play:

```sql
-- connect to the auth DB and set your server's reachable address
UPDATE realmlist SET address = '<server-ip>' WHERE id = 1;
```

---

## In-game admin menu addon

`client-addons/TCAdmin/` is a client addon that gives you an in-game window of
GM commands (teleport, items, NPC spawn, server controls, …). Clicking a button
runs the command on the server and streams the output back.

It uses TrinityCore's **native addon command channel** (addon prefix
`TrinityCore`), so the server enforces your account's GM permissions and **no
worldserver rebuild is required**. Copy the folder to
`<WoW client>/Interface/AddOns/TCAdmin/`, log in on a GM account, and type
`/tca`. See [`client-addons/TCAdmin/README.md`](client-addons/TCAdmin/README.md)
for details and how to customise the command list.

> The addon channel is enabled by default (`Addon.Channel = 1` in
> `worldserver.conf`); the deployment keeps that default.

## Configuration

All tunables live in `.env` (copied from `.env.example`). The container
entrypoints generate `worldserver.conf` / `authserver.conf` from the installed
`.dist` templates and override only the connection, data-dir, logging and
auto-updater settings from the environment — so upstream config defaults are
preserved across versions.

| Variable                    | Default      | Purpose                                  |
|-----------------------------|--------------|------------------------------------------|
| `TC_BRANCH`                 | `master`     | TrinityCore branch to build              |
| `BUILD_JOBS`                | all cores    | Cap parallel compile jobs (RAM control)  |
| `MYSQL_ROOT_PASSWORD`       | `rootpass`   | MySQL root password                      |
| `TC_DB_USER` / `_PASSWORD`  | `trinity`    | Server DB account                        |
| `TC_TDB_URL`                | _(empty)_    | TDB world DB download URL                 |
| `WOW_CLIENT_DIR`            | `./wow-client` | Path to your WoW client                |
| `MYSQL_PORT` / `AUTH_PORT` / `WORLD_PORT` | 3306 / 3724 / 8085 | Host port mappings    |

---

## Repository layout

```
.
├── tc                              # convenience CLI wrapper (./tc help)
├── docker-compose.yml              # the stack
├── .env.example                    # configuration template
├── docker/
│   ├── trinitycore/Dockerfile      # multi-stage build (servers + tools)
│   ├── common/entrypoint-lib.sh    # shared entrypoint helpers
│   ├── authserver/entrypoint.sh    # authserver config + launch
│   ├── worldserver/entrypoint.sh   # worldserver config, TDB, launch
│   └── mysql/init/                 # DB + user bootstrap (first init only)
├── scripts/
│   └── extract-client-data.sh      # client data extraction
├── client-addons/
│   └── TCAdmin/                    # in-game GM/admin command menu addon
└── data/                           # extracted client data (gitignored)
```

---

## Common operations

```bash
docker compose ps                       # status
docker compose logs -f worldserver      # follow world server logs
docker compose down                     # stop (keeps volumes/DB)
docker compose down -v                  # stop and DELETE the database volume
docker compose build --no-cache authserver   # rebuild from scratch
```

---

## Troubleshooting

- **worldserver exits / "maps not found":** client data isn't in `./data`. Run
  the extractor step.
- **DB connection refused:** the first MySQL init takes a moment; the servers
  wait for it, but a fresh build also needs the init script to finish. Check
  `docker compose logs db`.
- **TDB import skipped:** ensure `TC_TDB_URL` points at a `.7z` whose version
  matches your branch; delete the `world` database (`docker compose down -v`) to
  re-trigger a clean import.
- **Out of memory during build:** set `BUILD_JOBS` (e.g. `4`) in `.env`.

---

## Legal

TrinityCore is licensed under the GPL v2. World of Warcraft and its client data
are © Blizzard Entertainment. This repository contains **no** Blizzard
intellectual property; you must supply your own legally-obtained client. Running
a private server may violate Blizzard's Terms of Service — use at your own risk
and for educational/private purposes only.
