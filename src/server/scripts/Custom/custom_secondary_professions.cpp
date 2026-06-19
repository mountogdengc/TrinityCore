/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Custom feature: grant the secondary professions (Cooking, Fishing, Archaeology) to a
// character on its first login.
//
// Secondary skills are NOT bound by the client's two primary-profession slots
// (ActivePlayerData::ProfessionSkillLine), so they appear and work normally in the profession
// UI. Primary professions are intentionally not granted here: the modern client only exposes
// two primary-profession slots, so granting more would just create professions the UI cannot
// show or use.
//
// For each profession we learn its base ("Apprentice") spell, which grants the skill at level 1
// (1/75), adds the starter recipes, and registers the usable profession/trade-skill entry.
// LearnDefaultSkill is kept only as a fallback for builds where a base spell id is missing.
//
// Confirm the base spell ids against your world DB if anything is missing:
//   SELECT * FROM spell_learn_skill WHERE SkillID = <id>;

#include "DB2Stores.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include <array>

namespace
{
    // Flip to true to (re)grant on every login instead of only first login - useful to top up
    // characters that already existed before this script was installed. Leave false for normal use.
    constexpr bool SECONDARY_PROFESSIONS_GRANT_ON_EVERY_LOGIN = false;

    struct ProfessionEntry
    {
        uint32 SkillId;
        uint32 LearnSpellId;    // base "Apprentice <profession>" spell
    };

    constexpr std::array<ProfessionEntry, 3> SecondaryProfessions =
    { {
        { SKILL_COOKING,     2550  },
        { SKILL_FISHING,     7620  },
        { SKILL_ARCHAEOLOGY, 78670 }
    } };
}

class custom_secondary_professions : public PlayerScript
{
public:
    custom_secondary_professions() : PlayerScript("custom_secondary_professions") { }

    void OnLogin(Player* player, bool firstLogin) override
    {
        if (!firstLogin && !SECONDARY_PROFESSIONS_GRANT_ON_EVERY_LOGIN)
            return;

        for (ProfessionEntry const& profession : SecondaryProfessions)
        {
            if (player->HasSkill(profession.SkillId))
                continue;

            // Learning the base profession spell grants the skill, the starter recipes and the
            // usable spellbook/trade-skill entry.
            if (profession.LearnSpellId && sSpellMgr->GetSpellInfo(profession.LearnSpellId, DIFFICULTY_NONE))
                player->LearnSpell(profession.LearnSpellId, false);

            // Fallback: if the base spell is missing on this build, at least add the skill bar
            // (initialized to level 1 from the skill's race/class definition).
            if (!player->HasSkill(profession.SkillId))
                if (SkillRaceClassInfoEntry const* rcInfo = sDB2Manager.GetSkillRaceClassInfo(profession.SkillId, player->GetRace(), player->GetClass()))
                    player->LearnDefaultSkill(rcInfo);
        }
    }
};

void AddSC_custom_secondary_professions()
{
    new custom_secondary_professions();
}
