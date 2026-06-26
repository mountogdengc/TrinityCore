/*
 * Player-bot pet policy helpers (pure, unit-tested).
 */

#ifndef TRINITYCORE_BOTS_BOTPETPOLICY_H
#define TRINITYCORE_BOTS_BOTPETPOLICY_H

#include "Define.h"

namespace BotPetPolicy
{
    // A living, in-world Hunter bot with no pet should summon one.
    bool ShouldSummonPet(bool isHunter, bool inWorld, bool botAlive, bool petExists);

    // A dead pet should be revived once it has been dead at least reviveDelayMs.
    bool ShouldRevivePet(bool petExists, bool petAlive, int32 deadMs, int32 reviveDelayMs);

    // Hunter pets don't auto-sync level; resync when the pet's level differs from the owner.
    bool NeedsLevelSync(uint8 petLevel, uint8 ownerLevel);
}

#endif // TRINITYCORE_BOTS_BOTPETPOLICY_H
