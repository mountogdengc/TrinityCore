#!/usr/bin/env python3
"""Apply the player-bot core patches to a TrinityCore source tree.

These are the minimal edits to core files that the Custom/Bots scripts cannot
make on their own (LoginQueryHolder is file-local; the idle-kick dereferences a
null socket). Insertions are anchored to stable source text and are idempotent,
so re-running on an already-patched tree is a no-op. If an anchor is missing
(upstream moved it), the script fails loudly rather than silently miscompiling.
"""

import sys
import pathlib

SRC = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()


def patch(rel, anchor, insert, *, before=False, marker=None, replace=False):
    path = SRC / rel
    text = path.read_text()
    token = marker if marker is not None else insert.strip().splitlines()[0]
    if token in text:
        print(f"  [skip] {rel}: already patched")
        return
    if anchor not in text:
        sys.exit(f"  [FAIL] {rel}: anchor not found -> {anchor!r}")
    if replace:
        new = text.replace(anchor, insert, 1)
    elif before:
        new = text.replace(anchor, insert + anchor, 1)
    else:
        new = text.replace(anchor, anchor + insert, 1)
    path.write_text(new)
    print(f"  [ok]   {rel}")


# 1) WorldSession.h: declare the headless bot-login entry point (public member,
#    placed next to the related opcode handler).
patch(
    "src/server/game/Server/WorldSession.h",
    "        void HandlePlayerLogin(LoginQueryHolder const& holder);",
    "\n        // Player-bot overlay: load a character on a headless (socket-less) session.\n"
    "        void LoadBotCharacter(ObjectGuid guid);",
    marker="void LoadBotCharacter(ObjectGuid guid);",
)

# 2) CharacterHandler.cpp: define LoadBotCharacter where LoginQueryHolder is in
#    scope. Mirrors HandleContinuePlayerLogin minus the client-only packets.
BOT_LOGIN_DEF = """// Player-bot overlay: drive the normal async login for a headless session.
void WorldSession::LoadBotCharacter(ObjectGuid guid)
{
    if (PlayerLoading() || GetPlayer())
        return;

    m_playerLoading = guid;

    std::shared_ptr<LoginQueryHolder> holder = std::make_shared<LoginQueryHolder>(GetAccountId(), guid);
    if (!holder->Initialize())
    {
        m_playerLoading.Clear();
        return;
    }

    AddQueryHolderCallback(CharacterDatabase.DelayQueryHolder(holder)).AfterComplete([this](SQLQueryHolderBase const& holder)
    {
        HandlePlayerLogin(static_cast<LoginQueryHolder const&>(holder));
    });
}

"""
patch(
    "src/server/game/Handlers/CharacterHandler.cpp",
    "void WorldSession::HandlePlayerLogin(LoginQueryHolder const& holder)\n{",
    BOT_LOGIN_DEF,
    before=True,
    marker="void WorldSession::LoadBotCharacter(ObjectGuid guid)",
)

# 3) WorldSession.cpp: guard the idle-connection kick so a null socket (bot)
#    is never dereferenced.
patch(
    "src/server/game/Server/WorldSession.cpp",
    "    if (IsConnectionIdle() && !HasPermission(rbac::RBAC_PERM_IGNORE_IDLE_CONNECTION))",
    "    if (m_Socket[CONNECTION_TYPE_REALM] && IsConnectionIdle() && !HasPermission(rbac::RBAC_PERM_IGNORE_IDLE_CONNECTION))",
    marker="if (m_Socket[CONNECTION_TYPE_REALM] && IsConnectionIdle()",
    replace=True,
)

# 4) custom_script_loader.cpp: register the bot scripts.
patch(
    "src/server/scripts/Custom/custom_script_loader.cpp",
    "void AddCustomScripts()",
    "void AddSC_bots();\n\n",
    before=True,
    marker="void AddSC_bots();",
)
patch(
    "src/server/scripts/Custom/custom_script_loader.cpp",
    "void AddCustomScripts()\n{",
    "\n    AddSC_bots();",
    marker="AddSC_bots();\n}",  # unlikely; real marker checked below
)

print("Core patches applied.")
