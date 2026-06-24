#ifndef TRINITY_TIRISFAL_RECRUITMENT_H
#define TRINITY_TIRISFAL_RECRUITMENT_H

#include <cstdint>

#include "QuestDef.h"

namespace Tirisfal::Recruitment
{
enum class DarnellAssistSource : uint8
{
    None,
    MasterVictim,
    MasterSelectedTarget,
    MasterAttacker,
    SelfAttacker
};

bool IsDarnellQuest(uint32 questId);
bool ShouldEnsureDarnell(QuestStatus status);
bool ShouldCleanupDarnell(QuestStatus status);
bool CanGrantRecruitmentCredit(QuestStatus status, bool hasOwner, bool alreadyCredited);
DarnellAssistSource SelectDarnellAssistSource(bool hasMasterVictim, bool masterInCombat, bool hasMasterSelectedTarget, bool hasMasterAttacker, bool hasSelfAttacker);
bool ShouldScanRecruitmentCorpses(QuestStatus status);
bool CanCollectRecruitmentCorpse(QuestStatus status, bool alreadyCredited);
}

#endif
