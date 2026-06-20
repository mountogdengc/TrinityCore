/*
 * Player-bot cohort policy helpers.
 */

#include "BotCohortPolicy.h"

namespace BotCohortPolicy
{
BotLevelBandState ComputeLevelBandState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance)
{
    if (companionLevel + tolerance < ownerLevel)
        return BotLevelBandState::Below;

    if (companionLevel > ownerLevel + tolerance)
        return BotLevelBandState::Above;

    return BotLevelBandState::InRange;
}

BotCatchupState ComputeCatchupState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance, float boostedMultiplier)
{
    BotCatchupState result;
    result.BandState = ComputeLevelBandState(ownerLevel, companionLevel, tolerance);
    result.CatchupActive = result.BandState == BotLevelBandState::Below;
    result.XpMultiplier = result.CatchupActive ? boostedMultiplier : 1.0f;
    return result;
}

bool ShouldAutoSpawnCohort(bool autoSpawnEnabled, bool ownerAlive, bool ownerInWorld)
{
    return autoSpawnEnabled && ownerAlive && ownerInWorld;
}

bool ShouldAutoAcceptResurrection(bool isDead, bool hasRequest)
{
    return isDead && hasRequest;
}

bool ShouldAutoAcceptSummon(bool ownerInWorld, bool hasSummonPending)
{
    return ownerInWorld && hasSummonPending;
}
}
