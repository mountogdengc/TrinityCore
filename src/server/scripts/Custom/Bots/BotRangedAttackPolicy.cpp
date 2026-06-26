/*
 * Player-bot ranged auto-attack policy -- see BotRangedAttackPolicy.h.
 */

#include "BotRangedAttackPolicy.h"

namespace BotRangedAttackPolicy
{
bool ShouldStartAutoRepeat(bool hasAutoSpell, bool alreadyRepeating, bool targetChanged)
{
    return hasAutoSpell && (!alreadyRepeating || targetChanged);
}
}
