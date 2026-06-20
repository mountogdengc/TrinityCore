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

#include "ScriptMgr.h"
#include "Player.h"
#include "PlayerChoice.h"
#include "ScriptedCreature.h"
#include "TemporarySummon.h"
#include "tirisfal_recruitment.h"

#include <unordered_set>

enum ALegendYouCanHoldPriest
{
    // Spells
    SPELL_FORCE_HOLY_SPEC                               = 199701,
    SPELL_FORCE_SHADOW_SPEC                             = 199703,
    SPELL_FORCE_DISCIPLINE_SPEC                         = 199704,
    SPELL_PLAYERCHOICE_REMOVE_TRACKING_QUESTS_PRIEST    = 199699,

    // Playerchoices
    PLAYERCHOICE_RESPONSE_CHOOSE_HOLY_WEAPON            = 532,
    PLAYERCHOICE_RESPONSE_CHOOSE_SHADOW_WEAPON          = 533,
    PLAYERCHOICE_RESPONSE_CHOOSE_DISCIPLINE             = 534
};

enum RecruitmentQuests
{
    QUEST_BEYOND_THE_GRAVES                            = 25089,
    QUEST_RECRUITMENT                                  = 26800
};

enum RecruitmentCreatures
{
    NPC_DARNELL_RECRUITMENT                            = 49337,
    NPC_SCARLET_CORPSE_RECRUITMENT                     = 49340
};

namespace
{
Position const DarnellSummonPosition = { 1692.47f, 1653.24f, 130.32f, 0.0f };

bool HasActiveDarnellQuest(Player* player)
{
    return Tirisfal::Recruitment::ShouldEnsureDarnell(player->GetQuestStatus(QUEST_BEYOND_THE_GRAVES)) ||
        Tirisfal::Recruitment::ShouldEnsureDarnell(player->GetQuestStatus(QUEST_RECRUITMENT));
}

Creature* FindOwnedDarnell(Player* player, float range = 200.0f)
{
    return player->FindNearestCreatureWithOptions(range, { .CreatureId = NPC_DARNELL_RECRUITMENT, .IsSummon = true, .IgnorePhases = true, .OwnerGuid = player->GetGUID(), .PrivateObjectOwnerGuid = player->GetGUID() });
}

void EnsureDarnell(Player* player)
{
    if (!HasActiveDarnellQuest(player) || FindOwnedDarnell(player))
        return;

    player->SummonCreature(NPC_DARNELL_RECRUITMENT, DarnellSummonPosition, TEMPSUMMON_MANUAL_DESPAWN, 0s, 0, 0, player->GetGUID());
}

void CleanupDarnell(Player* player)
{
    if (Creature* darnell = FindOwnedDarnell(player, 500.0f))
        darnell->DespawnOrUnsummon();
}
}

// 248 - Playerchoice
class playerchoice_a_weapon_you_can_hold_priest : public PlayerChoiceScript
{
public:
    playerchoice_a_weapon_you_can_hold_priest() : PlayerChoiceScript("playerchoice_a_weapon_you_can_hold_priest") {}

    void OnResponse(WorldObject* /*object*/, Player* player, PlayerChoice const* /*choice*/, PlayerChoiceResponse const* response, uint16 /*clientIdentifier*/)
    {
        if (response->ResponseId == PLAYERCHOICE_RESPONSE_CHOOSE_HOLY_WEAPON)
            player->CastSpell(player, SPELL_FORCE_HOLY_SPEC, CastSpellExtraArgsInit{ .TriggerFlags = TRIGGERED_FULL_MASK });
        else if (response->ResponseId == PLAYERCHOICE_RESPONSE_CHOOSE_SHADOW_WEAPON)
            player->CastSpell(player, SPELL_FORCE_SHADOW_SPEC, CastSpellExtraArgsInit{ .TriggerFlags = TRIGGERED_FULL_MASK });
        else if (response->ResponseId == PLAYERCHOICE_RESPONSE_CHOOSE_DISCIPLINE)
            player->CastSpell(player, SPELL_FORCE_DISCIPLINE_SPEC, CastSpellExtraArgsInit{ .TriggerFlags = TRIGGERED_FULL_MASK });
    }
};

// 40706 - A Legend You Can Hold
class quest_a_legend_you_can_hold : public QuestScript
{
public:
    quest_a_legend_you_can_hold() : QuestScript("quest_a_legend_you_can_hold") {}

    void OnQuestStatusChange(Player* player, Quest const* /*quest*/, QuestStatus /*oldStatus*/, QuestStatus newStatus) override
    {
        if (newStatus == QUEST_STATUS_NONE)
            player->CastSpell(player, SPELL_PLAYERCHOICE_REMOVE_TRACKING_QUESTS_PRIEST, CastSpellExtraArgsInit{ .TriggerFlags = TRIGGERED_FULL_MASK });
    }
};

class player_tirisfal_recruitment_darnell : public PlayerScript
{
public:
    player_tirisfal_recruitment_darnell() : PlayerScript("player_tirisfal_recruitment_darnell") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        EnsureDarnell(player);
    }

    void OnPlayerRepop(Player* player) override
    {
        EnsureDarnell(player);
    }

    void OnQuestStatusChange(Player* player, uint32 questId) override
    {
        if (!Tirisfal::Recruitment::IsDarnellQuest(questId))
            return;

        if (HasActiveDarnellQuest(player))
            EnsureDarnell(player);
        else
            CleanupDarnell(player);
    }
};

struct npc_scarlet_corpse_recruitment : public ScriptedAI
{
    npc_scarlet_corpse_recruitment(Creature* creature) : ScriptedAI(creature) { }

    void MoveInLineOfSight(Unit* who) override
    {
        ScriptedAI::MoveInLineOfSight(who);

        TempSummon* darnell = who->ToTempSummon();
        if (!darnell || darnell->GetEntry() != NPC_DARNELL_RECRUITMENT)
            return;

        Player* owner = darnell->GetSummonerUnit() ? darnell->GetSummonerUnit()->ToPlayer() : nullptr;
        if (!owner)
            return;

        ObjectGuid ownerGuid = owner->GetGUID();
        bool alreadyCredited = _creditedOwners.find(ownerGuid) != _creditedOwners.end();
        if (!Tirisfal::Recruitment::CanGrantRecruitmentCredit(owner->GetQuestStatus(QUEST_RECRUITMENT), true, alreadyCredited))
            return;

        owner->KilledMonsterCredit(NPC_SCARLET_CORPSE_RECRUITMENT, me->GetGUID());
        _creditedOwners.insert(ownerGuid);
    }

private:
    GuidUnorderedSet _creditedOwners;
};

void AddSC_tirisfal_glades()
{
    // Playerchoice
    new playerchoice_a_weapon_you_can_hold_priest();
    new player_tirisfal_recruitment_darnell();

    // Quest
    new quest_a_legend_you_can_hold();
    RegisterCreatureAI(npc_scarlet_corpse_recruitment);
}
