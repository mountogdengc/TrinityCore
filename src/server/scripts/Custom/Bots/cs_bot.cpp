/*
 * Player-bot support for TrinityCore (master) - Milestone 1.
 *
 * Defines:
 *   - bot_commandscript : the ".bot add/remove/list" chat commands
 *   - bot_worldscript   : pumps BotMgr each world tick and cleans up on shutdown
 */

#include "ScriptMgr.h"
#include "BotMgr.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Player.h"
#include "RBAC.h"

using namespace Trinity::ChatCommands;

class bot_commandscript : public CommandScript
{
public:
    bot_commandscript() : CommandScript("bot_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable botCommandTable =
        {
            { "add",    HandleBotAddCommand,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "remove", HandleBotRemoveCommand, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "follow", HandleBotFollowCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            { "stay",   HandleBotStayCommand,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "count",  HandleBotCountCommand,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "bot", botCommandTable },
        };

        return commandTable;
    }

private:
    static bool HandleBotAddCommand(ChatHandler* handler, std::string const& name)
    {
        // Added in-world => the bot follows you; added from console => holds position.
        Player* owner = handler->GetPlayer();
        ObjectGuid const master = owner ? owner->GetGUID() : ObjectGuid::Empty;

        std::string error;
        if (!sBotMgr->AddBot(name, master, error))
        {
            handler->PSendSysMessage("Bot add failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Spawning bot '%s'...", name.c_str());
        return true;
    }

    static bool HandleBotFollowCommand(ChatHandler* handler, std::string const& name)
    {
        Player* owner = handler->GetPlayer();
        if (!owner)
        {
            handler->SendSysMessage("Run .bot follow from in-world; the bot follows you.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        std::string error;
        if (!sBotMgr->SetMaster(name, owner->GetGUID(), error))
        {
            handler->PSendSysMessage("Bot follow failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Bot '%s' is now following you.", name.c_str());
        return true;
    }

    static bool HandleBotStayCommand(ChatHandler* handler, std::string const& name)
    {
        std::string error;
        if (!sBotMgr->SetMaster(name, ObjectGuid::Empty, error))
        {
            handler->PSendSysMessage("Bot stay failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Bot '%s' will hold position.", name.c_str());
        return true;
    }

    static bool HandleBotRemoveCommand(ChatHandler* handler, std::string const& name)
    {
        std::string error;
        if (!sBotMgr->RemoveBot(name, error))
        {
            handler->PSendSysMessage("Bot remove failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Removed bot '%s'.", name.c_str());
        return true;
    }

    static bool HandleBotCountCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("Active bots: %zu", sBotMgr->GetBotCount());
        return true;
    }
};

class bot_worldscript : public WorldScript
{
public:
    bot_worldscript() : WorldScript("bot_worldscript") { }

    void OnUpdate(uint32 diff) override
    {
        sBotMgr->Update(diff);
    }

    void OnShutdown() override
    {
        sBotMgr->RemoveAllBots();
    }
};

// Registered from Custom/custom_script_loader.cpp (see AddCustomScripts).
void AddSC_bots()
{
    new bot_commandscript();
    new bot_worldscript();
}
