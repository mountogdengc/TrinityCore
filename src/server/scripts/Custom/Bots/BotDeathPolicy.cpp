/*
 * Custom: Death QoL — pure decision helpers (no engine state, unit-testable).
 * See docs/superpowers/specs/2026-06-20-death-qol-design.md
 */

#include "BotDeathPolicy.h"

#include <cctype>
#include <cstring>

namespace Bots::DeathPolicy
{
bool ShouldBotAutoRevive(bool enabled, bool botIsDead, bool masterIsAlive,
    uint32_t msSinceDeath, uint32_t delayMs)
{
    return enabled && botIsDead && masterIsAlive && msSinceDeath >= delayMs;
}

bool ShouldPlayerAutoRevive(bool enabled, bool isInWorld, bool isDead,
    bool isArenaOrBattleground, uint32_t msSinceDeath, uint32_t delayMs)
{
    return enabled && isInWorld && isDead && !isArenaOrBattleground && msSinceDeath >= delayMs;
}

bool IsReviveCorpseArg(char const* args)
{
    if (!args)
        return false;

    // Trim leading whitespace.
    while (*args && std::isspace(static_cast<unsigned char>(*args)))
        ++args;

    char const* word = "corpse";
    while (*word && *args && std::tolower(static_cast<unsigned char>(*args)) == *word)
    {
        ++args;
        ++word;
    }

    if (*word != '\0')
        return false;   // ran out of input before matching all of "corpse"

    // Trailing characters must be whitespace only.
    while (*args)
    {
        if (!std::isspace(static_cast<unsigned char>(*args)))
            return false;
        ++args;
    }
    return true;
}
}
