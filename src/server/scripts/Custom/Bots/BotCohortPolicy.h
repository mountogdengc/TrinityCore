/*
 * Player-bot cohort policy helpers.
 */

#ifndef TRINITYCORE_BOTS_BOTCOHORTPOLICY_H
#define TRINITYCORE_BOTS_BOTCOHORTPOLICY_H

#include "Define.h"

enum class BotLevelBandState : uint8
{
    Below,
    InRange,
    Above
};

struct BotCatchupState
{
    BotLevelBandState BandState = BotLevelBandState::InRange;
    float XpMultiplier = 1.0f;
    bool CatchupActive = false;
};

namespace BotCohortPolicy
{
BotLevelBandState ComputeLevelBandState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance);
BotCatchupState ComputeCatchupState(uint8 ownerLevel, uint8 companionLevel, uint8 tolerance, float boostedMultiplier);
bool ShouldAutoSpawnCohort(bool autoSpawnEnabled, bool ownerAlive, bool ownerInWorld);
bool ShouldAutoAcceptResurrection(bool isDead, bool hasRequest);
bool ShouldAutoAcceptSummon(bool ownerInWorld, bool hasSummonPending);
}

#endif // TRINITYCORE_BOTS_BOTCOHORTPOLICY_H
