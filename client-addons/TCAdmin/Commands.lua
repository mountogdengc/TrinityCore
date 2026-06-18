--[[
  Commands.lua -- the command catalog for the TC Admin menu.

  Each entry is data only; the UI renders a button per entry and Core.lua sends
  it over the addon command channel. Commands are written WITHOUT the leading
  dot, because the server's addon command handler parses them directly.

  Fields:
    label  (string)  Button text.
    cmd    (string)  Command sent to the server (no leading ".").
    input  (string)  Optional. If set, prompts for text that is appended to cmd.
    confirm(bool)     Optional. If true, ask for confirmation before sending.

  The security check is server-side: the account's GM rank / RBAC permissions
  decide what actually runs. Non-GM accounts simply get "command not found".

  Add your own categories/entries freely -- no reload of the server is needed.
]]

local _, TCAdmin = ...

TCAdmin.catalog = {
    {
        category = "Teleport",
        entries = {
            { label = "Stormwind",       cmd = "tele stormwind" },
            { label = "Orgrimmar",       cmd = "tele orgrimmar" },
            { label = "Dalaran",         cmd = "tele dalaran" },
            { label = "Shattrath",       cmd = "tele shattrath" },
            { label = "Named Location...", cmd = "tele",   input = "Location name" },
            { label = "Go To Player...", cmd = "appear",   input = "Player name" },
            { label = "Summon Player...",cmd = "summon",   input = "Player name" },
            { label = "Recall (last)",   cmd = "recall" },
        },
    },
    {
        category = "GM Tools",
        entries = {
            { label = "GM Mode On",      cmd = "gm on" },
            { label = "GM Mode Off",     cmd = "gm off" },
            { label = "GM Visible Off",  cmd = "gm visible off" },
            { label = "GM Visible On",   cmd = "gm visible on" },
            { label = "Fly On",          cmd = "fly on" },
            { label = "Fly Off",         cmd = "fly off" },
            { label = "God/Cheat On",    cmd = "cheat god on" },
            { label = "God/Cheat Off",   cmd = "cheat god off" },
            { label = "Speed...",        cmd = "modify speed", input = "Speed multiplier (1-50)" },
            { label = "Revive",          cmd = "revive" },
        },
    },
    {
        category = "Character",
        entries = {
            { label = "Level Up...",     cmd = "levelup",   input = "Levels to add (blank = 1)" },
            { label = "Max Skills",      cmd = "maxskill" },
            { label = "Repair Items",    cmd = "repairitems" },
            { label = "Learn Class Spells", cmd = "learn all_myclass" },
            { label = "Kill Self",       cmd = "die",       confirm = true },
        },
    },
    {
        category = "Items",
        entries = {
            { label = "Add Item...",     cmd = "additem",    input = "Item ID [count]" },
            { label = "Add Item Set...", cmd = "additemset", input = "Item set ID" },
            { label = "Give Money...",   cmd = "modify money", input = "Copper amount" },
        },
    },
    {
        category = "NPC",
        entries = {
            { label = "Spawn NPC...",    cmd = "npc add",  input = "Creature ID" },
            { label = "Delete Target",   cmd = "npc delete", confirm = true },
            { label = "Target Info",     cmd = "npc info" },
        },
    },
    {
        category = "Server",
        entries = {
            { label = "Server Info",     cmd = "server info" },
            { label = "Save All",        cmd = "saveall" },
            { label = "Announce...",     cmd = "announce",        input = "Message" },
            { label = "Set MOTD...",     cmd = "server set motd", input = "Message" },
        },
    },
}
