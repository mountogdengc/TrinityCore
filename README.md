# TrinityCore Fork Docker Workflow

A reproducible [Docker Compose](https://docs.docker.com/compose/) stack that
builds and runs the [TrinityCore](https://www.trinitycore.org/) source checked
out in this repository.

This repository is a full TrinityCore fork. Docker builds the currently
checked-out branch from local source, so any local changes in this fork are
compiled directly into the `authserver`, `worldserver`, and extraction tools.

The stack runs three long-lived services:

| Service | Role | Port |
| --- | --- | --- |
| `db` | MySQL 8.0 for `auth`, `characters`, and `world` | 3306 |
| `authserver` | Login and realm server | 3724 |
| `worldserver` | Game world server with console | 8085 |

It also provides an `extractor` helper for generating maps, vmaps, mmaps, and
other client-derived data from a local WoW client.

## What you still need to provide

This repository cannot include Blizzard assets. To run a playable server you
still need:

1. A legally obtained WoW client matching the TrinityCore branch you checked
out.
2. Extracted client data in `./data` generated from that client.
3. A TDB package that matches the checked-out TrinityCore branch, unless you
plan to import a world database manually.

## Prerequisites

- Docker Engine with the Docker Compose plugin
- Enough RAM and disk for a full TrinityCore build and extracted client data
- Network access for package installation during image build and optional TDB
  download on first worldserver start

## Quick Start

```bash
# 1. Configure the local environment
cp .env.example .env

# 2. Adjust .env:
#    - set TC_TDB_URL to a TDB release matching the branch checked out here
#    - set WOW_CLIENT_DIR to your WoW client path
#    - change the default database passwords

# 3. Build the local fork and start MySQL
docker compose build
docker compose up -d db

# 4. Extract client data from your WoW client
WOW_CLIENT_DIR=/path/to/WoW docker compose run --rm extractor

# 5. Start the TrinityCore servers
docker compose up -d authserver worldserver

# 6. Follow worldserver startup
docker compose logs -f worldserver
```

On first boot, `worldserver` creates and updates the databases, optionally
downloads the configured TDB archive, and imports the world database.

## Branching Model

The checked-out git branch in this repository determines what Docker builds.
There is no in-image clone of `TrinityCore/TrinityCore` anymore.

That means:

- switching branches changes the source Docker compiles
- local commits and uncommitted source edits are included in `docker compose build`
- future playerbot changes can be developed and tested directly in this fork

## TDB Selection

Set `TC_TDB_URL` in `.env` to a TDB archive matching the TrinityCore branch you
have checked out. Release assets are published on the
[TrinityCore releases page](https://github.com/TrinityCore/TrinityCore/releases).

Example:

```env
TC_TDB_URL=https://github.com/TrinityCore/TrinityCore/releases/download/TDB1200.26021/TDB_full_world_1200.26021_2026_02_01.7z
```

If `TC_TDB_URL` is left empty, the stack expects you to import the `world`
database yourself.

## Client Data Extraction

Run the one-off extractor helper against your WoW client:

```bash
WOW_CLIENT_DIR=/path/to/WoW docker compose run --rm extractor
```

This runs `mapextractor`, `vmap4extractor`, `vmap4assembler`, and
`mmaps_generator`, then moves the generated files into `./data`.

The extraction tools write intermediate files into the mounted client
directory, so use a client path that is writable.

## Creating A Game Account

Once `worldserver` is running:

```bash
docker attach tc-worldserver
```

At the `TC>` prompt:

```text
account create <username> <password>
account set gmlevel <username> 3 -1
```

Detach without stopping the container with `Ctrl-p`, then `Ctrl-q`.

## Configuration

Primary settings live in `.env`:

| Variable | Default | Purpose |
| --- | --- | --- |
| `BUILD_JOBS` | all cores | Limit compile parallelism |
| `MYSQL_ROOT_PASSWORD` | `rootpass` | MySQL root password |
| `TC_DB_USER` / `TC_DB_PASSWORD` | `trinity` | TrinityCore DB credentials |
| `TC_AUTH_DB` | `auth` | Auth database name |
| `TC_CHARACTERS_DB` | `characters` | Characters database name |
| `TC_WORLD_DB` | `world` | World database name |
| `TC_TDB_URL` | empty | Optional TDB download URL |
| `WOW_CLIENT_DIR` | `./wow-client` | Local WoW client path |
| `MYSQL_PORT` | `3306` | Host MySQL port |
| `AUTH_PORT` | `3724` | Host authserver port |
| `WORLD_PORT` | `8085` | Host worldserver port |

## Repository Additions

The fork adds these wrapper-owned files on top of upstream TrinityCore:

```text
docker-compose.yml
docker/
scripts/extract-client-data.sh
.env.example
```

The main TrinityCore source remains in the upstream layout at the repository
root.

## Common Operations

```bash
docker compose ps
docker compose logs -f worldserver
docker compose down
docker compose down -v
docker compose build --no-cache authserver
```

## Troubleshooting

- If `worldserver` reports missing maps, run the extractor step and verify
  files exist under `./data`.
- If database startup fails, inspect `docker compose logs db`.
- If TDB import is skipped, verify the `TC_TDB_URL` version matches the
  checked-out TrinityCore branch.
- If builds run out of memory, set `BUILD_JOBS` to a smaller value in `.env`.

## Legal

TrinityCore is GPLv2. World of Warcraft and its client files remain Blizzard
intellectual property. This repository ships no Blizzard assets.
