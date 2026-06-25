/*
 * Player-bot M4 rotation engine -- see BotRotation.h for the design rationale.
 */

#include "BotRotation.h"
#include "BotRotationPolicy.h"
#include "DB2Stores.h"
#include "DB2Structure.h"
#include "Map.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellDefines.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    // ChrSpecializationID -> the spec's ability priority list (spell ids, highest
    // priority first). Built lazily on first use and cached for the world's
    // lifetime: the Assisted Combat DB2 data is static (hotfix) reference data.
    // BotMgr drives this from the single world-update thread, so no locking.
    struct RuleData { int32 conditionType; int32 value1; int32 value2; int32 value3; };

    // spec -> ordered [(spellId, stepId)]; stepId lets a candidate find its rules.
    std::unordered_map<int32, std::vector<std::pair<uint32, int32>>> g_specPriority;
    // custom stepId (>= 1000000) -> fork rules gating that step.
    std::unordered_map<int32, std::vector<RuleData>> g_stepRules;
    bool g_indexBuilt = false;

    // Custom Assisted Combat rows (our hotfix data) use IDs >= this base.
    constexpr int32 BOT_CUSTOM_ID_BASE = 1000000;

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
        // from the steps, ordered by OrderIndex. AssistedCombatRule rows layer
        // ConditionType / ConditionValueN gating on top of steps: for our CUSTOM
        // steps (ID >= BOT_CUSTOM_ID_BASE) we evaluate fork opcodes (see
        // EvaluateStepConditions); Blizzard's stock-step opcodes are undocumented and
        // left fail-open (treated as eligible).
        // spec -> [(order, spellId, stepId)]
        std::unordered_map<int32, std::vector<std::tuple<int32, uint32, int32>>> specSteps;
        for (AssistedCombatStepEntry const* step : sAssistedCombatStepStore)
        {
            if (step->SpellID <= 0)
                continue;

            auto specItr = containerSpec.find(uint32(step->AssistedCombatID));
            if (specItr == containerSpec.end())
                continue;

            specSteps[specItr->second].emplace_back(step->OrderIndex, uint32(step->SpellID), int32(step->ID));
        }

        // Fork condition rules, indexed by step -- only for our custom steps.
        for (AssistedCombatRuleEntry const* rule : sAssistedCombatRuleStore)
        {
            if (rule->AssistedCombatStepID < BOT_CUSTOM_ID_BASE)
                continue;   // Blizzard step -> fail-open (no fork interpretation).
            g_stepRules[rule->AssistedCombatStepID].push_back(
                { rule->ConditionType, rule->ConditionValue1, rule->ConditionValue2, rule->ConditionValue3 });
        }

        for (auto& [specId, steps] : specSteps)
        {
            std::stable_sort(steps.begin(), steps.end(),
                [](std::tuple<int32, uint32, int32> const& a, std::tuple<int32, uint32, int32> const& b)
                { return std::get<0>(a) < std::get<0>(b); });

            std::vector<std::pair<uint32, int32>>& priority = g_specPriority[specId];
            for (auto const& [order, spellId, stepId] : steps)
            {
                bool seen = false;
                for (auto const& existing : priority)
                    if (existing.first == spellId) { seen = true; break; }
                if (!seen)
                    priority.emplace_back(spellId, stepId);   // keep highest-priority occurrence
            }
        }
    }

    // True if `target` still warrants casting `stepSpellId` from this step. No fork
    // rules -> always true. Unknown opcodes -> true (fail-open). Drives the
    // "don't re-apply an active DoT" gate.
    bool EvaluateStepConditions(Player* bot, Unit* target, int32 stepId, uint32 stepSpellId)
    {
        auto itr = g_stepRules.find(stepId);
        if (itr == g_stepRules.end())
            return true;

        for (RuleData const& rule : itr->second)
        {
            switch (rule.conditionType)
            {
                case BotRotationPolicy::BOT_COND_TARGET_MISSING_OR_EXPIRING_MY_AURA:
                {
                    uint32 const auraId = rule.value1 ? uint32(rule.value1) : stepSpellId;
                    Aura const* aura = target->GetAura(auraId, bot->GetGUID());
                    int32 const remainingMs = aura ? aura->GetDuration() : 0;
                    if (!BotRotationPolicy::ShouldCastForMissingOrExpiringAura(aura != nullptr, remainingMs, rule.value2))
                        return false;
                    break;
                }
                default:
                    break;   // unknown fork opcode -> fail-open
            }
        }
        return true;
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

    for (auto const& [spellId, stepId] : specItr->second)
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

        if (!EvaluateStepConditions(bot, target, stepId, spellId))
            continue;

        return spellId;
    }

    return 0;
}
