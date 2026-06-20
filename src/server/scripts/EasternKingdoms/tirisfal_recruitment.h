#ifndef TRINITY_TIRISFAL_RECRUITMENT_H
#define TRINITY_TIRISFAL_RECRUITMENT_H

#include <cstdint>

#include "QuestDef.h"

namespace Tirisfal::Recruitment
{
bool IsDarnellQuest(uint32 questId);
bool ShouldEnsureDarnell(QuestStatus status);
bool ShouldCleanupDarnell(QuestStatus status);
bool CanGrantRecruitmentCredit(QuestStatus status, bool hasOwner, bool alreadyCredited);
}

#endif
