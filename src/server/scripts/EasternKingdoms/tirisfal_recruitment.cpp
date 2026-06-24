#include "tirisfal_recruitment.h"

namespace Tirisfal::Recruitment
{
bool IsDarnellQuest(uint32 questId)
{
    return questId == 25089 || questId == 26800;
}

bool ShouldEnsureDarnell(QuestStatus status)
{
    return status == QUEST_STATUS_INCOMPLETE;
}

bool ShouldCleanupDarnell(QuestStatus status)
{
    return status == QUEST_STATUS_NONE || status == QUEST_STATUS_REWARDED;
}

bool CanGrantRecruitmentCredit(QuestStatus status, bool hasOwner, bool alreadyCredited)
{
    return status == QUEST_STATUS_INCOMPLETE && hasOwner && !alreadyCredited;
}

DarnellAssistSource SelectDarnellAssistSource(bool hasMasterVictim, bool masterInCombat, bool hasMasterSelectedTarget, bool hasMasterAttacker, bool hasSelfAttacker)
{
    if (hasMasterVictim)
        return DarnellAssistSource::MasterVictim;

    if (masterInCombat && hasMasterSelectedTarget)
        return DarnellAssistSource::MasterSelectedTarget;

    if (hasMasterAttacker)
        return DarnellAssistSource::MasterAttacker;

    if (hasSelfAttacker)
        return DarnellAssistSource::SelfAttacker;

    return DarnellAssistSource::None;
}

bool ShouldScanRecruitmentCorpses(QuestStatus status)
{
    return status == QUEST_STATUS_INCOMPLETE;
}

bool CanCollectRecruitmentCorpse(QuestStatus status, bool alreadyCredited)
{
    return ShouldScanRecruitmentCorpses(status) && !alreadyCredited;
}
}
