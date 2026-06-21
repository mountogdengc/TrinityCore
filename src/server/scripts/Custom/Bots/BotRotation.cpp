/*
 * Player-bot M4 rotation engine -- see BotRotation.h for the design rationale.
 */

#include "BotRotation.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Map.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellDefines.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    // ChrSpecializationID -> the spec's ability priority list (spell ids, highest
    // priority first). Built lazily on first use and cached for the world's
    // lifetime: the Assisted Combat DB2 data is static (hotfix) reference data.
    // BotMgr drives this from the single world-update thread, so no locking.
    std::unordered_map<int32, std::vector<uint32>> g_specPriority;
    bool g_indexBuilt = false;

    void EnsureIndex()
    {
        if (g_indexBuilt)
            return;
        g_indexBuilt = true;

        // AssistedCombat (ID, ChrSpecializationID): one container per spec.
        std::unordered_map<uint32, int32> containerSpec;
        for (AssistedCombatEntry const* entry : sAssistedCombatStore)
            containerSpec[entry->ID] = entry->ChrSpecializationID;

        // AssistedCombatStep (SpellID, AssistedCombatID -> container, OrderIndex)
        // IS the spec's ordered ability list. We build the priority list straight
        // from the steps, ordered by OrderIndex. The AssistedCombatRule rows layer
        // ConditionType / ConditionValueN gating on top of steps -- intentionally
        // NOT evaluated on this first pass (decoding Blizzard's condition opcodes is
        // a follow-on slice). Sourcing from steps (not rules) is also what the
        // low-level Hunter hotfix spike provides: it ships steps with no rules.
        std::unordered_map<int32, std::vector<std::pair<int32, uint32>>> specSteps; // spec -> [(order, spellId)]
        for (AssistedCombatStepEntry const* step : sAssistedCombatStepStore)
        {
            if (step->SpellID <= 0)
                continue;

            auto specItr = containerSpec.find(uint32(step->AssistedCombatID));
            if (specItr == containerSpec.end())
                continue;

            specSteps[specItr->second].emplace_back(step->OrderIndex, uint32(step->SpellID));
        }

        for (auto& [specId, steps] : specSteps)
        {
            std::stable_sort(steps.begin(), steps.end(),
                [](std::pair<int32, uint32> const& a, std::pair<int32, uint32> const& b)
                { return a.first < b.first; });

            std::vector<uint32>& priority = g_specPriority[specId];
            for (std::pair<int32, uint32> const& step : steps)
                if (std::find(priority.begin(), priority.end(), step.second) == priority.end())
                    priority.push_back(step.second);   // keep the first (highest-priority) occurrence
        }
    }
}

uint32 BotRotation::SelectSpell(Player* bot, Unit* target)
{
    if (!bot || !target)
        return 0;

    // Don't stack casts: let the current one resolve. Melee/movement continue.
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return 0;

    EnsureIndex();

    auto specItr = g_specPriority.find(int32(bot->GetPrimarySpecialization()));
    if (specItr == g_specPriority.end())
        return 0;   // no spec / no Assisted Combat data -> caller melees.

    Difficulty const difficulty = bot->GetMap()->GetDifficultyID();
    SpellHistory* history = bot->GetSpellHistory();
    float const distance = bot->GetExactDist(target);

    for (uint32 spellId : specItr->second)
    {
        if (!bot->HasSpell(spellId))
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId, difficulty);
        if (!spellInfo)
            continue;

        // Cooldown / category cooldown / charges, and the global cooldown.
        if (history->HasGlobalCooldown(spellInfo) || !history->IsReady(spellInfo))
            continue;

        // Resource cost (mana / energy / rage / runic power / combo points / ...).
        bool affordable = true;
        for (SpellPowerCost const& cost : spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask()))
        {
            if (cost.Amount > bot->GetPower(cost.Power))
            {
                affordable = false;
                break;
            }
        }
        if (!affordable)
            continue;

        // Range: a (near-)zero max range is a melee strike, so require melee reach;
        // otherwise honor the spell's max range to the victim.
        float const maxRange = spellInfo->GetMaxRange(false, bot);
        if (maxRange <= 1.0f)
        {
            if (!bot->IsWithinMeleeRange(target))
                continue;
        }
        else if (distance > maxRange)
            continue;

        return spellId;
    }

    return 0;
}
