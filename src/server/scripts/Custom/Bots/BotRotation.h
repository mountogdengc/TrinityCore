/*
 * Player-bot M4 rotation engine.
 *
 * A server-side resolver for Blizzard's "Assisted Combat" (Single-Button
 * Assistant) per-spec ability priority lists, which retail resolves client-side
 * and a headless bot therefore can't use. It walks the bot spec's priority list
 * (from the AssistedCombat* DB2 stores) and returns the highest-priority ability
 * the bot can actually cast right now.
 *
 * First pass = "castability-priority": we honor the priority ORDER but gate each
 * candidate on real cast mechanics (known + off cooldown/GCD + affordable + in
 * range). The rules' ConditionType / ConditionValueN columns are intentionally
 * NOT evaluated yet -- decoding Blizzard's condition opcodes is a follow-on slice.
 */

#ifndef TRINITYCORE_BOTS_BOTROTATION_H
#define TRINITYCORE_BOTS_BOTROTATION_H

#include "Define.h"

class Player;
class Unit;

namespace BotRotation
{
    // Highest-priority Assisted Combat ability `bot` can cast at `target` this
    // tick, or 0 when nothing is castable (the caller falls back to melee). Returns
    // 0 for a bot with no spec / no Assisted Combat data, or while already casting.
    uint32 SelectSpell(Player* bot, Unit* target);
}

#endif // TRINITYCORE_BOTS_BOTROTATION_H
