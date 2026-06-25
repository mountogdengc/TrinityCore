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

// Custom feature: grant every weapon skill to a character so any class can wield any weapon type
// (e.g. a mage swinging a polearm) from level 1.
//
// Retail no longer *levels* weapon skills - the old 1..300 bar that raised by swinging and drove
// misses/glancing is gone. Combat in this core uses GetMaxSkillValueForLevel() (level * 5) for both
// attacker and victim (see Unit::RollMeleeOutcomeAgainst), so the stored skill value is irrelevant
// to hit/damage. What *does* survive is weapon skills as a binary equip gate: Player::CanUseItem
// rejects a weapon whose skill line the character lacks (GetSkillValue(itemSkill) == 0 ->
// EQUIP_ERR_PROFICIENCY_NEEDED). A class simply isn't granted the off-class weapon skill lines, so
// the server refuses the equip. Granting them here removes that gate.
//
// Two things are needed for an item to actually go on:
//   1. The skill line itself - the server-side equip gate above. Persisted, so granted once.
//   2. The weapon proficiency bitmask (m_WeaponProficiency) - the client-side gate the paperdoll
//      uses. It is never persisted (only initialised to 0 in the Player ctor and rebuilt from the
//      proficiency spells a character knows), so we re-assert it on *every* login for whatever
//      weapon skills the character has - both the ones we grant and the ones that came with the
//      class.
//
// Secondary professions (Cooking/Fishing/Archaeology) are handled separately in
// custom_secondary_professions.cpp; fishing poles are intentionally not part of this list.

#include "ItemTemplate.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include <array>

namespace
{
    // Flip to true to (re)grant on every login instead of only first login - useful to top up
    // characters that already existed before this script was installed. Leave false for normal use.
    constexpr bool WEAPON_SKILLS_GRANT_ON_EVERY_LOGIN = false;

    struct WeaponSkillEntry
    {
        uint32 SkillId;
        uint32 SubClass;    // ITEM_SUBCLASS_WEAPON_* - the bit set in the client proficiency mask
    };

    // Skill <-> weapon-subclass pairing mirrors ItemTemplate::GetSkill()'s ItemWeaponSkills table,
    // which is the authoritative mapping CanUseItem checks against. Subclasses that map to no skill
    // (thrown, spears, exotics, miscellaneous) have no equip gate and are left out.
    constexpr std::array<WeaponSkillEntry, 15> WeaponSkills =
    { {
        { SKILL_AXES,              ITEM_SUBCLASS_WEAPON_AXE         },
        { SKILL_TWO_HANDED_AXES,   ITEM_SUBCLASS_WEAPON_AXE2        },
        { SKILL_BOWS,              ITEM_SUBCLASS_WEAPON_BOW         },
        { SKILL_GUNS,              ITEM_SUBCLASS_WEAPON_GUN         },
        { SKILL_MACES,             ITEM_SUBCLASS_WEAPON_MACE        },
        { SKILL_TWO_HANDED_MACES,  ITEM_SUBCLASS_WEAPON_MACE2       },
        { SKILL_POLEARMS,          ITEM_SUBCLASS_WEAPON_POLEARM     },
        { SKILL_SWORDS,            ITEM_SUBCLASS_WEAPON_SWORD       },
        { SKILL_TWO_HANDED_SWORDS, ITEM_SUBCLASS_WEAPON_SWORD2      },
        { SKILL_WARGLAIVES,        ITEM_SUBCLASS_WEAPON_WARGLAIVES  },
        { SKILL_STAVES,            ITEM_SUBCLASS_WEAPON_STAFF       },
        { SKILL_FIST_WEAPONS,      ITEM_SUBCLASS_WEAPON_FIST_WEAPON },
        { SKILL_DAGGERS,           ITEM_SUBCLASS_WEAPON_DAGGER      },
        { SKILL_CROSSBOWS,         ITEM_SUBCLASS_WEAPON_CROSSBOW    },
        { SKILL_WANDS,             ITEM_SUBCLASS_WEAPON_WAND        }
    } };
}

class custom_weapon_skills : public PlayerScript
{
public:
    custom_weapon_skills() : PlayerScript("custom_weapon_skills") { }

    void OnLogin(Player* player, bool firstLogin) override
    {
        bool const grantMissing = firstLogin || WEAPON_SKILLS_GRANT_ON_EVERY_LOGIN;

        uint32 proficiencyMask = 0;

        for (WeaponSkillEntry const& weapon : WeaponSkills)
        {
            if (!player->HasSkill(weapon.SkillId))
            {
                if (!grantMissing)
                    continue;

                // All weapon skills are level-range (SKILL_RANGE_LEVEL): value 1, max scaling with
                // level - exactly how the core grants a class's own weapon skills in
                // LearnDefaultSkill. The value never feeds combat, so an off-class weapon performs
                // identically to a trained one; value 1 only needs to clear the > 0 equip gate.
                player->SetSkill(weapon.SkillId, 0, 1, player->GetMaxSkillValueForLevel());
            }

            // Mirror the (now present) skill line into the client proficiency bitmask. Done for
            // every weapon the character has, every login, because the bitmask is not persisted.
            proficiencyMask |= (1u << weapon.SubClass);
        }

        if (proficiencyMask && (player->GetWeaponProficiency() & proficiencyMask) != proficiencyMask)
        {
            player->AddWeaponProficiency(proficiencyMask);
            player->SendProficiency(ITEM_CLASS_WEAPON, player->GetWeaponProficiency());
        }
    }
};

void AddSC_custom_weapon_skills()
{
    new custom_weapon_skills();
}
