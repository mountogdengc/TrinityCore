--[[
  Core.lua -- protocol layer for the native TrinityCore addon command channel.

  Wire protocol (server: AddonChannelCommandHandler, prefix "TrinityCore"):
    Client -> server, addon message body:
        <opcode><4-byte echo token><command text without leading dot>
      opcode 'h' = issue human-readable command
      opcode 'i' = issue machine-readable command
      opcode 'p' = ping
    Server -> client, addon message body (same prefix), first char = opcode,
    next 4 bytes echo the token, remainder is the body:
      'a' = acknowledged      (no body)
      'o' = command OK / done (no body)
      'f' = command failed    (no body)
      'm' = message line      (body = one line of output; '|' escaped as '||')

  We send over the WHISPER channel to ourselves; the server intercepts messages
  with this prefix before they are delivered, so nothing is actually whispered.
]]

local ADDON_NAME, TCAdmin = ...

local PREFIX = "TrinityCore"
TCAdmin.PREFIX = PREFIX

-- Per-command state keyed by the 4-char echo token.
local pending = {}

-- Generate a 4-character token from a safe alphabet so the server can echo it
-- back verbatim without any byte-encoding surprises.
local TOKEN_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
local function newToken()
    local t = {}
    for i = 1, 4 do
        local n = math.random(#TOKEN_CHARS)
        t[i] = TOKEN_CHARS:sub(n, n)
    end
    return table.concat(t)
end

-- onLine(text)        called once per 'm' output line
-- onDone(success)     called once on 'o' (true) or 'f' (false)
function TCAdmin:SendCommand(command, callbacks)
    callbacks = callbacks or {}
    if not command or command == "" then return end

    local token = newToken()
    pending[token] = {
        command = command,
        onLine  = callbacks.onLine,
        onDone  = callbacks.onDone,
    }

    -- 'h' = human-readable output, followed by the 4-byte token, then the cmd.
    local body = "h" .. token .. command
    local target = UnitName("player")
    C_ChatInfo.SendAddonMessage(PREFIX, body, "WHISPER", target)
    return token
end

local function handleMessage(text)
    if not text or #text < 5 then return end
    local opcode = text:sub(1, 1)
    local token  = text:sub(2, 5)
    local body   = text:sub(6)

    local req = pending[token]
    if not req then return end  -- not ours / already finished

    if opcode == "m" then
        -- Unescape the '||' -> '|' the server applied to message bodies.
        body = body:gsub("||", "|")
        if req.onLine then req.onLine(body) end
    elseif opcode == "o" then
        if req.onDone then req.onDone(true) end
        pending[token] = nil
    elseif opcode == "f" then
        if req.onDone then req.onDone(false) end
        pending[token] = nil
    elseif opcode == "a" then
        -- acknowledged; nothing to do but keep waiting for output / result
    end
end

local frame = CreateFrame("Frame")
frame:RegisterEvent("PLAYER_LOGIN")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:SetScript("OnEvent", function(_, event, ...)
    if event == "PLAYER_LOGIN" then
        if C_ChatInfo and C_ChatInfo.RegisterAddonMessagePrefix then
            C_ChatInfo.RegisterAddonMessagePrefix(PREFIX)
        end
        TCAdminDB = TCAdminDB or {}
        if TCAdmin.OnLogin then TCAdmin:OnLogin() end
    elseif event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        if prefix == PREFIX then
            handleMessage(message)
        end
    end
end)
