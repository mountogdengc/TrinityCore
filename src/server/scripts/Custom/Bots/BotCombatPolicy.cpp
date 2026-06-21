/*
 * Player-bot combat policy helpers.
 */

#include "BotCombatPolicy.h"

namespace BotCombatPolicy
{
bool ShouldUseMasterSelectedTarget(bool masterHasEligibleVictim, bool masterInCombat, bool selectedTargetEligible)
{
    // Only assist the master's *selected* target once the master is actually in
    // combat -- otherwise merely clicking/selecting a hostile would make the bot
    // attack it. The master's real victim is handled separately (and engages the
    // instant the master starts attacking, so little pre-pull speed is lost).
    return !masterHasEligibleVictim && masterInCombat && selectedTargetEligible;
}

bool ShouldKeepCurrentVictim(bool hasReplacementTarget, bool victimAlive, bool victimStillValid,
    bool victimWithinLeash, uint32 staleCombatTimer, uint32 staleCombatTimeout)
{
    if (hasReplacementTarget)
        return false;

    if (!victimAlive)
        return false;

    if (victimStillValid && victimWithinLeash)
        return true;

    return staleCombatTimer < staleCombatTimeout;
}
}
