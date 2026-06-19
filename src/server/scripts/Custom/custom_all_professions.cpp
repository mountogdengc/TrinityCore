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

// Custom feature: grant every profession to a character on its first login.
//
// For each profession we learn its base ("Apprentice") spell. Learning that spell:
//   - grants the skill (cascaded via spell_learn_skill), starting at level 1 (1/75),
//   - adds the starter/apprentice recipes,
//   - makes the profession appear in the spellbook's Professions tab and opens its
//     trade-skill window, so it is actually usable.
// If a base spell id is missing on this build we fall back to LearnDefaultSkill so the
// skill bar still appears (no recipes / may not be usable until the correct spell is learned).
//
// Notes:
//  - Primary professions are bound by the client's two primary-profession slots
//    (ActivePlayerData::ProfessionSkillLine has only two entries). Professions beyond the first
//    two are fully learned and gatherable/craftable, but only two ever occupy a "primary
//    profession" UI slot - verify the profession book behaves acceptably on your client build.
//  - Secondary skills (Cooking, Fishing, Archaeology) are not slot- or cap-bound.
//  - Only newly created characters are affected (firstLogin). Set
//    ALL_PROFESSIONS_GRANT_ON_EVERY_LOGIN below to top up characters that already existed.
//  - The base spell ids below are the long-standing apprentice spells; confirm them against
//    your world DB if anything is missing: SELECT * FROM spell_learn_skill WHERE SkillID = <id>;

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
    constexpr bool ALL_PROFESSIONS_GRANT_ON_EVERY_LOGIN = false;

    struct ProfessionEntry
    {
        uint32 SkillId;
        uint32 LearnSpellId;    // base "Apprentice <profession>" spell
    };

    constexpr std::array<ProfessionEntry, 14> Professions =
    { {
        // Primary professions (limited by the two ProfessionSkillLine UI slots)
        { SKILL_ALCHEMY,        2259  },
        { SKILL_BLACKSMITHING,  2018  },
        { SKILL_ENCHANTING,     7411  },
        { SKILL_ENGINEERING,    4036  },
        { SKILL_HERBALISM,      2366  },
        { SKILL_INSCRIPTION,    45357 },
        { SKILL_JEWELCRAFTING,  25229 },
        { SKILL_LEATHERWORKING, 2108  },
        { SKILL_MINING,         2575  },
        { SKILL_SKINNING,       8613  },
        { SKILL_TAILORING,      3908  },
        // Secondary professions
        { SKILL_COOKING,        2550  },
        { SKILL_FISHING,        7620  },
        { SKILL_ARCHAEOLOGY,    78670 }
    } };
}

class custom_all_professions : public PlayerScript
{
public:
    custom_all_professions() : PlayerScript("custom_all_professions") { }

    void OnLogin(Player* player, bool firstLogin) override
    {
        if (!firstLogin && !ALL_PROFESSIONS_GRANT_ON_EVERY_LOGIN)
            return;

        for (ProfessionEntry const& profession : Professions)
        {
            if (player->HasSkill(profession.SkillId))
                continue;

            // Learning the base profession spell grants the skill, the apprentice recipes and
            // the spellbook/trade-skill entry that makes the profession usable.
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

void AddSC_custom_all_professions()
{
    new custom_all_professions();
}
