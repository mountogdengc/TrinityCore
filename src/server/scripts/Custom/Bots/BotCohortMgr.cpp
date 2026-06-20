/*
 * Player-bot cohort persistence and owner-facing cohort actions.
 */

#include "BotCohortMgr.h"
#include "BotCohortPolicy.h"
#include "BotMgr.h"
#include "CharacterCache.h"
#include "CharacterDatabase.h"
#include "ObjectGuid.h"
#include "Player.h"
#include <string>
#include <vector>

BotCohortMgr* BotCohortMgr::instance()
{
    static BotCohortMgr instance;
    return &instance;
}

std::vector<BotCohortMgr::CohortMember> BotCohortMgr::LoadCompanions(ObjectGuid ownerGuid) const
{
    std::vector<CohortMember> companions;
    if (ownerGuid.IsEmpty())
        return companions;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_BOT_COHORT_BY_OWNER);
    stmt->setUInt64(0, ownerGuid.GetCounter());

    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
    {
        do
        {
            Field* fields = result->Fetch();

            CohortMember member;
            member.CompanionGuid = ObjectGuid::Create<HighGuid::Player>(fields[0].GetUInt64());
            member.Active = fields[1].GetBool();
            member.AutoSpawn = fields[2].GetBool();
            companions.push_back(member);
        } while (result->NextRow());
    }

    return companions;
}

bool BotCohortMgr::AssignCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, bool active, std::string& error)
{
    if (ownerGuid.IsEmpty())
    {
        error = "Owner is required.";
        return false;
    }

    if (companionGuid.IsEmpty())
    {
        error = "Companion is required.";
        return false;
    }

    if (ownerGuid == companionGuid)
    {
        error = "You cannot assign yourself as a companion.";
        return false;
    }

    if (!sCharacterCache->GetCharacterAccountIdByGuid(ownerGuid))
    {
        error = "Could not resolve the owner character.";
        return false;
    }

    if (!sCharacterCache->GetCharacterAccountIdByGuid(companionGuid))
    {
        error = "Could not resolve the companion character.";
        return false;
    }

    bool autoSpawnEnabled = GetAutoSpawnEnabled(ownerGuid);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_BOT_COHORT_MEMBER);
    stmt->setUInt64(0, ownerGuid.GetCounter());
    stmt->setUInt64(1, companionGuid.GetCounter());
    stmt->setBool(2, active);
    stmt->setBool(3, autoSpawnEnabled);
    CharacterDatabase.Execute(stmt);

    return true;
}

bool BotCohortMgr::RemoveCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, std::string& error)
{
    if (ownerGuid.IsEmpty())
    {
        error = "Owner is required.";
        return false;
    }

    if (companionGuid.IsEmpty())
    {
        error = "Companion is required.";
        return false;
    }

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_BOT_COHORT_MEMBER);
    stmt->setUInt64(0, ownerGuid.GetCounter());
    stmt->setUInt64(1, companionGuid.GetCounter());
    CharacterDatabase.Execute(stmt);
    return true;
}

std::vector<ObjectGuid> BotCohortMgr::GetActiveCompanions(ObjectGuid ownerGuid) const
{
    std::vector<ObjectGuid> activeCompanions;
    for (CohortMember const& member : LoadCompanions(ownerGuid))
        if (member.Active)
            activeCompanions.push_back(member.CompanionGuid);

    return activeCompanions;
}

bool BotCohortMgr::GetAutoSpawnEnabled(ObjectGuid ownerGuid) const
{
    for (CohortMember const& member : LoadCompanions(ownerGuid))
        if (member.AutoSpawn)
            return true;

    return false;
}

bool BotCohortMgr::SetAutoSpawnEnabled(ObjectGuid ownerGuid, bool enabled, std::string& error)
{
    std::vector<CohortMember> companions = LoadCompanions(ownerGuid);
    if (companions.empty())
    {
        error = "No cohort members are assigned.";
        return false;
    }

    for (CohortMember const& member : companions)
    {
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_BOT_COHORT_AUTO_SPAWN);
        stmt->setBool(0, enabled);
        stmt->setUInt64(1, ownerGuid.GetCounter());
        stmt->setUInt64(2, member.CompanionGuid.GetCounter());
        CharacterDatabase.Execute(stmt);
    }

    return true;
}

bool BotCohortMgr::SpawnOwnerCohort(Player* owner, std::string& error)
{
    if (!owner)
    {
        error = "Owner is required.";
        return false;
    }

    if (!BotCohortPolicy::ShouldAutoSpawnCohort(true, owner->IsAlive(), owner->IsInWorld()))
    {
        error = "You must be alive and in the world to spawn your cohort.";
        return false;
    }

    std::vector<ObjectGuid> companions = GetActiveCompanions(owner->GetGUID());
    if (companions.empty())
    {
        error = "No active cohort members are assigned.";
        return false;
    }

    for (ObjectGuid const& companionGuid : companions)
    {
        if (!sBotMgr->AddBot(companionGuid, owner->GetGUID(), error))
            return false;
    }

    return true;
}
