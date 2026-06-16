--[[
  UI.lua -- the admin menu window: category list, command buttons, output log.
]]

local ADDON_NAME, TCAdmin = ...

-- ------------------------------------------------------------------ dialogs
StaticPopupDialogs["TCADMIN_INPUT"] = {
    text = "%s",
    button1 = OKAY,
    button2 = CANCEL,
    hasEditBox = true,
    OnAccept = function(self)
        local val = self.editBox:GetText()
        if self.data and self.data.run then self.data.run(val) end
    end,
    EditBoxOnEnterPressed = function(self)
        local parent = self:GetParent()
        if parent.button1 then parent.button1:Click() end
    end,
    EditBoxOnEscapePressed = function(self) self:GetParent():Hide() end,
    timeout = 0, whileDead = true, hideOnEscape = true,
}

StaticPopupDialogs["TCADMIN_CONFIRM"] = {
    text = "Run command:  .%s  ?",
    button1 = YES,
    button2 = NO,
    OnAccept = function(self)
        if self.data and self.data.run then self.data.run() end
    end,
    timeout = 0, whileDead = true, hideOnEscape = true,
}

-- ------------------------------------------------------------------ window
local frame = CreateFrame("Frame", "TCAdminFrame", UIParent, "BasicFrameTemplateWithInset")
frame:SetSize(560, 520)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:SetClampedToScreen(true)
frame:Hide()
TCAdmin.frame = frame

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
title:SetPoint("TOP", 0, -4)
title:SetText("TC Admin Menu")

-- Raw command box: type any command (leading dot optional) and press Enter.
local cmdLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
cmdLabel:SetPoint("TOPLEFT", 14, -34)
cmdLabel:SetText("Command:")

local runBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
runBtn:SetSize(60, 22)
runBtn:SetPoint("TOPRIGHT", -14, -30)
runBtn:SetText("Run")

local cmdBox = CreateFrame("EditBox", nil, frame, "InputBoxTemplate")
cmdBox:SetSize(380, 20)
cmdBox:SetPoint("LEFT", cmdLabel, "RIGHT", 12, 0)
cmdBox:SetPoint("RIGHT", runBtn, "LEFT", -10, 0)
cmdBox:SetAutoFocus(false)
cmdBox:SetMaxLetters(240)  -- addon messages cap at 255 bytes incl. opcode+token

local function runRaw()
    local t = (cmdBox:GetText() or ""):gsub("^%s+", ""):gsub("%s+$", "")
    t = t:gsub("^%.", "")  -- tolerate a leading dot if the user types one
    if t ~= "" then
        TCAdmin:Execute(t)
    end
    cmdBox:SetText("")
    cmdBox:ClearFocus()
end

cmdBox:SetScript("OnEnterPressed", runRaw)
cmdBox:SetScript("OnEscapePressed", function(self) self:SetText(""); self:ClearFocus() end)
runBtn:SetScript("OnClick", runRaw)

-- Output log (newest at the bottom).
local out = CreateFrame("ScrollingMessageFrame", nil, frame)
out:SetSize(536, 120)
out:SetPoint("BOTTOMLEFT", 12, 36)
out:SetFontObject(GameFontHighlightSmall)
out:SetJustifyH("LEFT")
out:SetMaxLines(300)
out:SetFading(false)
out:SetInsertMode("BOTTOM")
out:SetHyperlinksEnabled(false)
out:EnableMouseWheel(true)
out:SetScript("OnMouseWheel", function(self, delta)
    if delta > 0 then self:ScrollUp() else self:ScrollDown() end
end)

local clearBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
clearBtn:SetSize(80, 20)
clearBtn:SetPoint("BOTTOMRIGHT", -12, 12)
clearBtn:SetText("Clear")
clearBtn:SetScript("OnClick", function() out:Clear() end)

local logLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
logLabel:SetPoint("BOTTOMLEFT", 12, 14)
logLabel:SetText("Output")

function TCAdmin:Log(text, r, g, b)
    out:AddMessage(text or "", r or 0.9, g or 0.9, b or 0.9)
end

-- ------------------------------------------------------------- run a command
function TCAdmin:Execute(command)
    self:Log("> ." .. command, 1.0, 0.82, 0.0)
    self:SendCommand(command, {
        onLine = function(line) self:Log(line, 0.85, 0.85, 0.85) end,
        onDone = function(ok)
            if ok then self:Log("[done]", 0.4, 1.0, 0.4)
            else        self:Log("[failed]", 1.0, 0.4, 0.4) end
        end,
    })
end

function TCAdmin:Activate(entry)
    if entry.input then
        StaticPopup_Show("TCADMIN_INPUT", entry.input, nil, {
            run = function(val)
                local cmd = entry.cmd
                if val and val ~= "" then cmd = cmd .. " " .. val end
                TCAdmin:Execute(cmd)
            end,
        })
    elseif entry.confirm then
        StaticPopup_Show("TCADMIN_CONFIRM", entry.cmd, nil, {
            run = function() TCAdmin:Execute(entry.cmd) end,
        })
    else
        self:Execute(entry.cmd)
    end
end

-- -------------------------------------------------------- category + buttons
local catButtons = {}
local cmdButtons = {}
local activeCat

local function buildCommandButtons(catIndex)
    activeCat = catIndex
    for _, b in ipairs(cmdButtons) do b:Hide() end
    for i, b in ipairs(catButtons) do
        if i == catIndex then b:LockHighlight() else b:UnlockHighlight() end
    end

    local entries = TCAdmin.catalog[catIndex].entries
    for i, entry in ipairs(entries) do
        local b = cmdButtons[i]
        if not b then
            b = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
            b:SetSize(280, 24)
            b:SetPoint("TOPLEFT", 160, -66 - (i - 1) * 26)
            cmdButtons[i] = b
        end
        b:SetText(entry.label)
        b:SetScript("OnClick", function() TCAdmin:Activate(entry) end)
        b:Show()
    end
end

local function buildCategories()
    for i, cat in ipairs(TCAdmin.catalog) do
        local b = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
        b:SetSize(130, 24)
        b:SetPoint("TOPLEFT", 12, -66 - (i - 1) * 26)
        b:SetText(cat.category)
        b:SetScript("OnClick", function() buildCommandButtons(i) end)
        catButtons[i] = b
    end
    buildCommandButtons(1)
end
buildCategories()

-- ----------------------------------------------------------- toggle + slash
function TCAdmin:Toggle()
    if frame:IsShown() then frame:Hide() else frame:Show() end
end

SLASH_TCADMIN1 = "/tcadmin"
SLASH_TCADMIN2 = "/tca"
SlashCmdList["TCADMIN"] = function(msg)
    msg = (msg or ""):gsub("^%s+", ""):gsub("%s+$", "")
    if msg == "" then
        TCAdmin:Toggle()
    else
        -- /tca <raw command>  -> send directly (e.g. "/tca server info")
        TCAdmin:Execute(msg)
        if not frame:IsShown() then frame:Show() end
    end
end

function TCAdmin:OnLogin()
    self:Log("TC Admin ready. Type /tca to toggle. Commands run with your", 0.6, 0.8, 1.0)
    self:Log("server-side GM permissions.", 0.6, 0.8, 1.0)
end
