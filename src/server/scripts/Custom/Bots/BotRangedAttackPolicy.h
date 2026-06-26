/*
 * Player-bot ranged auto-attack policy (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTRANGEDATTACKPOLICY_H
#define TRINITYCORE_BOTS_BOTRANGEDATTACKPOLICY_H

#include "Define.h"

namespace BotRangedAttackPolicy
{
    // (Re)start the ranged autorepeat (Auto Shot / wand Shoot) when the bot has an
    // auto-attack spell AND is either not currently repeating or just switched targets.
    bool ShouldStartAutoRepeat(bool hasAutoSpell, bool alreadyRepeating, bool targetChanged);
}

#endif // TRINITYCORE_BOTS_BOTRANGEDATTACKPOLICY_H
