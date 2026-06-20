/*
 * Player-bot support for TrinityCore (master) - Milestone 1.
 *
 * Defines:
 *   - bot_commandscript : the ".bot add/remove/list" chat commands
 *   - bot_worldscript   : pumps BotMgr each world tick and cleans up on shutdown
 */

#include "ScriptMgr.h"
#include "BotCohortMgr.h"
#include "BotMgr.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Player.h"
#include "RBAC.h"
#include <vector>

using namespace Trinity::ChatCommands;

class bot_commandscript : public CommandScript
{
public:
    bot_commandscript() : CommandScript("bot_commandscript") { }

    std::span<ChatCommandBuilder const> GetCommands() const override
    {
        static ChatCommandTable cohortCommandTable =
        {
            { "assign", HandleBotCohortAssignCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No },
            { "remove", HandleBotCohortRemoveCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No },
            { "list",   HandleBotCohortListCommand,   rbac::RBAC_PERM_COMMAND_GM, Console::No },
            { "auto",   HandleBotCohortAutoCommand,   rbac::RBAC_PERM_COMMAND_GM, Console::No },
            { "spawn",  HandleBotCohortSpawnCommand,  rbac::RBAC_PERM_COMMAND_GM, Console::No },
        };

        static ChatCommandTable botCommandTable =
        {
            { "add",    HandleBotAddCommand,    rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "remove", HandleBotRemoveCommand, rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "follow", HandleBotFollowCommand, rbac::RBAC_PERM_COMMAND_GM, Console::No  },
            { "stay",   HandleBotStayCommand,   rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "count",  HandleBotCountCommand,  rbac::RBAC_PERM_COMMAND_GM, Console::Yes },
            { "cohort", cohortCommandTable },
        };

        static ChatCommandTable commandTable =
        {
            { "bot", botCommandTable },
        };

        return commandTable;
    }

private:
    static Player* GetOwner(ChatHandler* handler)
    {
        Player* owner = handler->GetPlayer();
        if (!owner)
        {
            handler->SendSysMessage("Run this command from an in-world player.");
            handler->SetSentErrorMessage(true);
        }

        return owner;
    }

    static bool ResolveCharacterGuid(ChatHandler* handler, std::string const& name, ObjectGuid& guid)
    {
        guid = sCharacterCache->GetCharacterGuidByName(name);
        if (guid.IsEmpty())
        {
            handler->PSendSysMessage("No character named '%s' exists.", name.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

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

    static bool HandleBotCohortAssignCommand(ChatHandler* handler, std::string const& name)
    {
        Player* owner = GetOwner(handler);
        if (!owner)
            return false;

        ObjectGuid companionGuid;
        if (!ResolveCharacterGuid(handler, name, companionGuid))
            return false;

        std::string error;
        if (!sBotCohortMgr->AssignCompanion(owner->GetGUID(), companionGuid, true, error))
        {
            handler->PSendSysMessage("Bot cohort assign failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Assigned '%s' to your cohort.", name.c_str());
        return true;
    }

    static bool HandleBotCohortRemoveCommand(ChatHandler* handler, std::string const& name)
    {
        Player* owner = GetOwner(handler);
        if (!owner)
            return false;

        ObjectGuid companionGuid;
        if (!ResolveCharacterGuid(handler, name, companionGuid))
            return false;

        std::string error;
        if (!sBotCohortMgr->RemoveCompanion(owner->GetGUID(), companionGuid, error))
        {
            handler->PSendSysMessage("Bot cohort remove failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Removed '%s' from your cohort.", name.c_str());
        return true;
    }

    static bool HandleBotCohortListCommand(ChatHandler* handler)
    {
        Player* owner = GetOwner(handler);
        if (!owner)
            return false;

        std::vector<ObjectGuid> companions = sBotCohortMgr->GetActiveCompanions(owner->GetGUID());
        handler->PSendSysMessage("Cohort auto-spawn: %s", sBotCohortMgr->GetAutoSpawnEnabled(owner->GetGUID()) ? "on" : "off");
        handler->PSendSysMessage("Active cohort members: %zu", companions.size());

        if (companions.empty())
            return true;

        for (ObjectGuid const& companionGuid : companions)
        {
            std::string companionName;
            if (!sCharacterCache->GetCharacterNameByGuid(companionGuid, companionName))
                companionName = companionGuid.ToString();

            handler->PSendSysMessage("Active cohort companion: %s", companionName.c_str());
        }

        return true;
    }

    static bool HandleBotCohortAutoCommand(ChatHandler* handler, Optional<bool> enabled)
    {
        Player* owner = GetOwner(handler);
        if (!owner)
            return false;

        if (!enabled)
        {
            handler->PSendSysMessage("Cohort auto-spawn is %s.",
                sBotCohortMgr->GetAutoSpawnEnabled(owner->GetGUID()) ? "on" : "off");
            return true;
        }

        std::string error;
        if (!sBotCohortMgr->SetAutoSpawnEnabled(owner->GetGUID(), *enabled, error))
        {
            handler->PSendSysMessage("Bot cohort auto failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Cohort auto-spawn is now %s.", *enabled ? "on" : "off");
        return true;
    }

    static bool HandleBotCohortSpawnCommand(ChatHandler* handler)
    {
        Player* owner = GetOwner(handler);
        if (!owner)
            return false;

        std::string error;
        if (!sBotCohortMgr->SpawnOwnerCohort(owner, error))
        {
            handler->PSendSysMessage("Bot cohort spawn failed: %s", error.c_str());
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Spawned %zu cohort companion(s).",
            sBotCohortMgr->GetActiveCompanions(owner->GetGUID()).size());
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
