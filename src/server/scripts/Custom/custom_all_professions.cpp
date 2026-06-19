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

// Custom feature: grant every profession to a character on its first login, each starting at
// skill level 1 so it can be leveled normally.
//
// Notes:
//  - Primary professions are bound by the client's two primary-profession slots
//    (ActivePlayerData::ProfessionSkillLine has only two entries). Professions beyond the first
//    two are fully learned and craftable/gatherable, but only two ever occupy a "primary
//    profession" UI slot - verify the profession book behaves acceptably on your client build.
//  - Secondary skills (Cooking, Fishing, Archaeology) are not slot- or cap-bound.
//  - Only newly created characters are affected (firstLogin). Existing characters can be topped
//    up with a GM command or a one-off pass with ALL_PROFESSIONS_GRANT_ON_EVERY_LOGIN below.

#include "DB2Stores.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include <array>

namespace
{
    // Flip to true to (re)grant on every login instead of only first login - useful to top up
    // characters that already existed before this script was installed. Leave false for normal use.
    constexpr bool ALL_PROFESSIONS_GRANT_ON_EVERY_LOGIN = false;

    constexpr std::array<uint32, 14> AllProfessionSkills =
    {
        // Primary professions (limited by the two ProfessionSkillLine UI slots)
        SKILL_ALCHEMY, SKILL_BLACKSMITHING, SKILL_ENCHANTING, SKILL_ENGINEERING,
        SKILL_HERBALISM, SKILL_INSCRIPTION, SKILL_JEWELCRAFTING, SKILL_LEATHERWORKING,
        SKILL_MINING, SKILL_SKINNING, SKILL_TAILORING,
        // Secondary professions
        SKILL_COOKING, SKILL_FISHING, SKILL_ARCHAEOLOGY
    };
}

class custom_all_professions : public PlayerScript
{
public:
    custom_all_professions() : PlayerScript("custom_all_professions") { }

    void OnLogin(Player* player, bool firstLogin) override
    {
        if (!firstLogin && !ALL_PROFESSIONS_GRANT_ON_EVERY_LOGIN)
            return;

        for (uint32 skillId : AllProfessionSkills)
        {
            if (player->HasSkill(skillId))
                continue;

            // Initialize the profession exactly like a normally-learned one: LearnDefaultSkill
            // resolves the correct starting/maximum value for the skill's first tier (level 1)
            // from its race/class definition, so we never hardcode version-specific values.
            if (SkillRaceClassInfoEntry const* rcInfo = sDB2Manager.GetSkillRaceClassInfo(skillId, player->GetRace(), player->GetClass()))
                player->LearnDefaultSkill(rcInfo);
        }
    }
};

void AddSC_custom_all_professions()
{
    new custom_all_professions();
}
