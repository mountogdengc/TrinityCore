/*
 * Player-bot pet policy helpers -- see BotPetPolicy.h.
 */

#include "BotPetPolicy.h"

namespace BotPetPolicy
{
bool ShouldSummonPet(bool isHunter, bool inWorld, bool botAlive, bool petExists)
{
    return isHunter && inWorld && botAlive && !petExists;
}

bool ShouldRevivePet(bool petExists, bool petAlive, int32 deadMs, int32 reviveDelayMs)
{
    return petExists && !petAlive && deadMs >= reviveDelayMs;
}

bool NeedsLevelSync(uint8 petLevel, uint8 ownerLevel)
{
    return petLevel != ownerLevel;
}
}
