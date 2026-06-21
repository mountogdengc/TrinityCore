/*
 * Player-bot cohort persistence and owner-facing cohort actions.
 */

#ifndef TRINITYCORE_BOTS_BOTCOHORTMGR_H
#define TRINITYCORE_BOTS_BOTCOHORTMGR_H

#include "ObjectGuid.h"
#include <string>
#include <vector>

class Player;

class BotCohortMgr
{
public:
    static BotCohortMgr* instance();

    bool AssignCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, bool active, std::string& error);
    bool RemoveCompanion(ObjectGuid ownerGuid, ObjectGuid companionGuid, std::string& error);
    std::vector<ObjectGuid> GetActiveCompanions(ObjectGuid ownerGuid) const;
    bool GetAutoSpawnEnabled(ObjectGuid ownerGuid) const;
    bool SetAutoSpawnEnabled(ObjectGuid ownerGuid, bool enabled, std::string& error);
    bool SpawnOwnerCohort(Player* owner, std::string& error);

private:
    BotCohortMgr() = default;
    ~BotCohortMgr() = default;
    BotCohortMgr(BotCohortMgr const&) = delete;
    BotCohortMgr& operator=(BotCohortMgr const&) = delete;

    struct CohortMember
    {
        ObjectGuid CompanionGuid = ObjectGuid::Empty;
        bool Active = false;
        bool AutoSpawn = false;
    };

    std::vector<CohortMember> LoadCompanions(ObjectGuid ownerGuid) const;
};

#define sBotCohortMgr BotCohortMgr::instance()

#endif // TRINITYCORE_BOTS_BOTCOHORTMGR_H
