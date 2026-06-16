/*
 * Player-bot overlay for TrinityCore (master) - Milestone 1.
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
            { "add",    HandleBotAddCommand,    rbac::RBAC_PERM_COMMAND_GM, Console::No },
            { "remove", HandleBotRemoveCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No },
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
        std::string error;
        if (!sBotMgr->AddBot(name, error))
        {
            handler->PSendSysMessage("Bot add failed: {}", error);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Spawning bot '{}'...", name);
        return true;
    }

    static bool HandleBotRemoveCommand(ChatHandler* handler, std::string const& name)
    {
        std::string error;
        if (!sBotMgr->RemoveBot(name, error))
        {
            handler->PSendSysMessage("Bot remove failed: {}", error);
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Removed bot '{}'.", name);
        return true;
    }

    static bool HandleBotCountCommand(ChatHandler* handler)
    {
        handler->PSendSysMessage("Active bots: {}", sBotMgr->GetBotCount());
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

// Registered from Custom/custom_script_loader.cpp (see overlay apply step).
void AddSC_bots()
{
    new bot_commandscript();
    new bot_worldscript();
}
