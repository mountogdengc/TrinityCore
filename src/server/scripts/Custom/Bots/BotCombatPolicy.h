/*
 * Player-bot combat policy helpers.
 */

#ifndef TRINITYCORE_BOTS_BOTCOMBATPOLICY_H
#define TRINITYCORE_BOTS_BOTCOMBATPOLICY_H

#include "Define.h"

namespace BotCombatPolicy
{
bool ShouldUseMasterSelectedTarget(bool masterHasEligibleVictim, bool masterInCombat, bool selectedTargetEligible);

bool ShouldKeepCurrentVictim(bool hasReplacementTarget, bool victimAlive, bool victimStillValid,
    bool victimWithinLeash, uint32 staleCombatTimer, uint32 staleCombatTimeout);
}

#endif // TRINITYCORE_BOTS_BOTCOMBATPOLICY_H
