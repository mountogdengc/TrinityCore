# TC Admin Menu (client addon)

An in-game window with a menu of TrinityCore GM/admin commands. Clicking a
button runs the command on the server and streams the output back into the
addon — no chat typing, no client-side hacks.

## How it works (Approach B, but no custom server code)

TrinityCore's worldserver has a **built-in addon command channel**
(`AddonChannelCommandHandler`, addon prefix **`TrinityCore`**). This addon
speaks that native protocol:

- It sends each command as an addon message:
  `h<4-char token><command without leading dot>`.
- The server runs the command **with the account's own GM rank / RBAC
  permissions** and replies over the same prefix with acknowledge / output
  lines (`m`) / OK (`o`) / failed (`f`).

Because the server enforces permissions, this is safe to hand to any account:
non-GM players simply get "command not found". **No worldserver rebuild or C++
module is required** — it works against a stock `master` build.

## Requirements

- The worldserver must have the addon channel enabled — it is on by default
  (`Addon.Channel = 1` in `worldserver.conf`).
- The logged-in account needs the appropriate **GM level / RBAC permissions**
  for the commands you want to run (e.g. `account set gmlevel <name> 3 -1`).
- The client must allow custom / out-of-date addons (enable "Load out of date
  AddOns" at the character-select AddOns screen).

## Install

Copy the `TCAdmin` folder into your WoW client's AddOns directory:

```
<WoW client>/Interface/AddOns/TCAdmin/
```

(so you have `Interface/AddOns/TCAdmin/TCAdmin.toc`). Restart the client or
`/reload`.

## Use

- `/tca` or `/tcadmin` — toggle the window.
- **Raw command box** at the top: type any command (the leading dot is
  optional) and press Enter or click **Run** — e.g. `additem 49623 1`. Useful
  for anything not in the catalog.
- **Recent commands** — click the `v` button next to the box for a dropdown of
  your recently-run commands; pick one to drop it back into the box (then edit
  and/or press Enter). The list ends with a *clear history* action. History is
  saved between sessions via the addon's `TCAdminDB` SavedVariable (last 50
  commands, deduplicated, most-recent first).
- Pick a category on the left, click a command on the right.
- Commands ending in `...` prompt for input (item ID, player name, message, …).
- `/tca <raw command>` — run a command directly, e.g. `/tca server info`.

Output (and any errors) appear in the log at the bottom of the window.

## Customising the command list

Edit `Commands.lua` — it is pure data. Add entries to a category, or add new
categories:

```lua
{ label = "Add Item...", cmd = "additem", input = "Item ID [count]" },
```

- `cmd` is the command **without** the leading dot.
- `input` (optional) prompts for text that is appended to `cmd`.
- `confirm = true` (optional) asks for confirmation first.

No server restart is needed; just `/reload` the client after editing.

## Notes / limits

- The interface version in `TCAdmin.toc` (`## Interface: 120005`) targets client
  **12.0.5**. Update it if your client build differs (it only affects the
  out-of-date warning).
- Addon messages are capped at 255 bytes, so keep individual commands short.
- This uses the same mechanism Blizzard's own UI uses for addon-issued
  commands, so it is robust against the chat-restriction issues that the
  simpler "send a `.command` via SendChatMessage" approach can hit on retail.
