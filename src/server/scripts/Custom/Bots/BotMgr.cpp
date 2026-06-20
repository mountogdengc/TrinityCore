/*
 * Player-bot support for TrinityCore (master).
 *   M1: spawn/despawn a real character as a headless bot.
 *   M2: follow a master player (MoveFollow), zone/teleport with them, stay alive.
 * See BotMgr.h for the headless-session design rationale.
 */

#include "BotMgr.h"
#include "BotCombatPolicy.h"
#include "CharacterCache.h"
#include "Chat.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "MotionMaster.h"
#include "MovementDefines.h"
#include "MovementPackets.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "Realm/ClientBuildInfo.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSocket.h"
#include <algorithm>
#include <cctype>
#include <memory>

namespace
{
    // How often the follow pass runs, the chase distance, and the gap past which
    // the bot blinks to the master instead of running (handles mount/taxi/teleport).
    constexpr uint32 BOT_FOLLOW_INTERVAL_MS = 500;
    constexpr float  BOT_FOLLOW_DIST        = 2.0f;
    constexpr float  BOT_CATCHUP_DIST       = 40.0f;
    constexpr uint32 BOT_POSTCOMBAT_HOLD_MS = 3000;  // linger after a fight before re-following
    constexpr uint32 BOT_STALE_COMBAT_MS    = 1500;
    constexpr float  BOT_COMBAT_LEASH_DIST  = 60.0f;

    std::string ToLower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return str;
    }

    bool IsEligibleMasterAssistTarget(Player* bot, Player* master, Unit* target)
    {
        if (!target || !target->IsAlive())
            return false;

        if (bot->GetMap() != target->GetMap() || !bot->InSamePhase(target))
            return false;

        if (bot->IsFriendlyTo(target) || target->IsFriendlyTo(bot))
            return false;

        if (bot->GetExactDist(target) > BOT_COMBAT_LEASH_DIST)
            return false;

        return master->IsValidAttackTarget(target);
    }
}

BotMgr* BotMgr::instance()
{
    static BotMgr instance;
    return &instance;
}

bool BotMgr::AddBot(std::string const& characterName, ObjectGuid master, std::string& error)
{
    std::string const key = ToLower(characterName);
    if (_bots.find(key) != _bots.end())
    {
        error = "A bot with that name is already active.";
        return false;
    }

    ObjectGuid const guid = sCharacterCache->GetCharacterGuidByName(characterName);
    if (guid.IsEmpty())
    {
        error = "No character named '" + characterName + "' exists.";
        return false;
    }

    if (ObjectAccessor::FindConnectedPlayer(guid))
    {
        error = "That character is already online.";
        return false;
    }

    uint32 const accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
    if (!accountId)
    {
        error = "Could not resolve the owning account for that character.";
        return false;
    }

    // Refuse to collide with a real (or already-bot) session on that account.
    if (sWorld->FindSession(accountId))
    {
        error = "The owning account already has an active session.";
        return false;
    }

    // Headless session: a null socket. SendPacket()/SendDirectMessage() null-check
    // the socket, so the login packet flow runs harmlessly with no client.
    WorldSession* session = new WorldSession(accountId, std::string("BOT"), 0u, std::string(),
        std::shared_ptr<WorldSocket>(),
        SEC_PLAYER, static_cast<uint8>(sWorld->getIntConfig(CONFIG_EXPANSION)),
        time_t(0), std::string(), Minutes(0),
        0u /*build*/, ClientBuild::VariantId{}, LOCALE_enUS,
        0u /*recruiter*/, false);

    _bots[key] = BotEntry{ session, master };

    // Reuse the real async login machinery (LoginQueryHolder -> HandlePlayerLogin).
    // The holder callback fires from this session's Update() in BotMgr::Update().
    session->LoadBotCharacter(guid);

    TC_LOG_INFO("bots", "BotMgr: spawning bot for character '{}' (account {}).", characterName, accountId);
    return true;
}

bool BotMgr::SetMaster(std::string const& characterName, ObjectGuid master, std::string& error)
{
    auto itr = _bots.find(ToLower(characterName));
    if (itr == _bots.end())
    {
        error = "No active bot with that name.";
        return false;
    }

    itr->second.master = master;
    return true;
}

bool BotMgr::RemoveBot(std::string const& characterName, std::string& error)
{
    std::string const key = ToLower(characterName);
    auto itr = _bots.find(key);
    if (itr == _bots.end())
    {
        error = "No active bot with that name.";
        return false;
    }

    WorldSession* session = itr->second.session;
    _bots.erase(itr);

    // Leave the master's party so it isn't left with a stale member.
    if (Player* bot = session->GetPlayer())
        if (Group* group = bot->GetGroup())
            group->RemoveMember(bot->GetGUID());

    // ~WorldSession() calls LogoutPlayer(true) when a player is still attached,
    // which saves the character and removes it from the map / ObjectAccessor.
    delete session;

    TC_LOG_INFO("bots", "BotMgr: removed bot '{}'.", characterName);
    return true;
}

void BotMgr::Update(uint32 diff)
{
    if (_bots.empty())
        return;

    for (auto const& [name, entry] : _bots)
    {
        // MapSessionFilter::ProcessUnsafe() == false, which skips the logout /
        // socket-cleanup block in WorldSession::Update() that would otherwise
        // return false (and signal deletion) for our socket-less session.
        MapSessionFilter filter(entry.session);
        entry.session->Update(diff, filter);
    }

    _followTimer += diff;
    if (_followTimer < BOT_FOLLOW_INTERVAL_MS)
        return;
    _followTimer = 0;
    UpdateFollow();
}

void BotMgr::UpdateFollow()
{
    for (auto& [name, entry] : _bots)
    {
        if (entry.master.IsEmpty())
            continue;   // M1 behaviour: no master => hold position.

        Player* bot = entry.session->GetPlayer();
        if (!bot)
            continue;

        Player* master = ObjectAccessor::FindConnectedPlayer(entry.master);
        WorldSession* ms = (master && master->IsInWorld()) ? master->GetSession() : nullptr;

        // A socket-less bot can't send the acks a client normally would, so we
        // drive the teleport state machine to completion ourselves. This MUST run
        // before the in-world check below: a cross-map port removes the bot from
        // the old map (IsInWorld()==false) before adding it to the new one, so if
        // we bailed on !IsInWorld() first the bot would strand mid-port and never
        // arrive.
        switch (bot->GetTeleportState())
        {
            case TeleportState::WaitingForTeleportAck:           // same-map (near) teleport
            {
                TC_LOG_DEBUG("bots", "Bot '{}' near-teleport ack (map {}).", bot->GetName(), bot->GetMapId());
                WorldPackets::Movement::MoveTeleportAck ack{ WorldPacket(uint32(CMSG_MOVE_TELEPORT_ACK)) };
                ack.MoverGUID = bot->GetGUID();
                entry.session->HandleMoveTeleportAck(ack);
                continue;
            }
            case TeleportState::WaitingForSuspendTokenResponse:  // cross-map step 1
            {
                TC_LOG_DEBUG("bots", "Bot '{}' suspend-token (map {}).", bot->GetName(), bot->GetMapId());
                WorldPackets::Movement::SuspendTokenResponse resp{ WorldPacket(uint32(CMSG_SUSPEND_TOKEN_RESPONSE)) };
                entry.session->HandleSuspendTokenResponse(resp);
                continue;
            }
            case TeleportState::WaitingForWorldPortAck:          // cross-map step 2
                TC_LOG_DEBUG("bots", "Bot '{}' worldport ack (map {}).", bot->GetName(), bot->GetMapId());
                entry.session->HandleMoveWorldportAck();
                continue;
            case TeleportState::NotTeleporting:
                break;                                            // free to act
            default:
                continue;                                         // Initiated/Delayed: wait a tick.
        }

        // Past the teleport machine: only follow/zone logic from here, which needs
        // a fully in-world, living bot.
        if (!bot->IsInWorld() || !bot->IsAlive())
            continue;

        if (!ms)
            continue;   // master logged off => bot just stands there.

        // M3: keep the bot in the master's party so they share the same instance.
        EnsureGrouped(bot, master);

        // Cross-map, or fell too far behind on the same map (mount / taxi / a
        // master teleport): blink to the master's position. Pass the master's
        // instance id so the bot joins the SAME instance (dungeons/raids).
        if (bot->GetMapId() != master->GetMapId() || bot->GetDistance(master) > BOT_CATCHUP_DIST)
        {
            uint32 const fromMap = bot->GetMapId();
            bool const ok = bot->TeleportTo(master->GetMapId(), master->GetPositionX(), master->GetPositionY(),
                master->GetPositionZ(), master->GetOrientation(), TELE_TO_NONE, master->GetInstanceId());
            if (ok)
                TC_LOG_DEBUG("bots", "Bot '{}' teleporting from map {} to {} (instance {}).",
                    bot->GetName(), fromMap, master->GetMapId(), master->GetInstanceId());
            else
                ChatHandler(ms).PSendSysMessage("Bot '%s' could not follow you to map %u.",
                    bot->GetName().c_str(), master->GetMapId());
            continue;
        }

        // M3: assist the master's target, or defend ourselves if attacked. Once
        // Attack()+MoveChase() are set the bot's own Unit::Update drives the melee
        // swings, so we just (re)issue them when the target changes. (M4 will layer
        // a real spell rotation on top of this melee baseline.)
        if (Unit* target = SelectAssistTarget(bot, master))
        {
            entry.staleCombatTimer = 0;
            entry.holdTimer = BOT_POSTCOMBAT_HOLD_MS;   // remember we were fighting
            if (bot->GetVictim() != target)
            {
                bot->Attack(target, true);
                bot->GetMotionMaster()->MoveChase(target);
            }
            continue;
        }

        // No fresh target this tick. Keep a valid current victim, and only force a
        // stop after a short stale window so transient target-selection gaps don't
        // snap the bot out of combat.
        if (Unit* victim = bot->GetVictim())
        {
            bool const victimStillValid = bot->IsValidAttackTarget(victim);
            bool const victimWithinLeash = bot->GetExactDist(victim) <= BOT_COMBAT_LEASH_DIST;
            if (victim->IsAlive() && victimStillValid && victimWithinLeash)
            {
                entry.staleCombatTimer = 0;
                continue;
            }

            bool const keepVictim = BotCombatPolicy::ShouldKeepCurrentVictim(false, victim->IsAlive(),
                victimStillValid, victimWithinLeash, entry.staleCombatTimer, BOT_STALE_COMBAT_MS);
            if (keepVictim)
            {
                entry.staleCombatTimer += BOT_FOLLOW_INTERVAL_MS;
                continue;
            }

            entry.staleCombatTimer = 0;
            bot->AttackStop();
        }
        else
            entry.staleCombatTimer = 0;

        if (entry.holdTimer)
        {
            entry.holdTimer = (entry.holdTimer > BOT_FOLLOW_INTERVAL_MS)
                ? entry.holdTimer - BOT_FOLLOW_INTERVAL_MS : 0;
            continue;
        }

        // Close enough => chase on foot. Re-issued only when the current generator
        // isn't a follow (initial, post-teleport, post-combat).
        if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
            bot->GetMotionMaster()->MoveFollow(master, BOT_FOLLOW_DIST);
    }
}

Unit* BotMgr::SelectAssistTarget(Player* bot, Player* master)
{
    Unit* const mVictim = master->GetVictim();
    Unit* const mSelected = ObjectAccessor::GetUnit(*master, master->GetTarget());

    // Assist: whatever the master is meleeing, or their currently selected
    // hostile target when there is no active master victim yet.
    bool const hasMasterVictim = IsEligibleMasterAssistTarget(bot, master, mVictim);
    if (hasMasterVictim)
        return mVictim;

    if (BotCombatPolicy::ShouldUseMasterSelectedTarget(hasMasterVictim, master->IsInCombat(),
        IsEligibleMasterAssistTarget(bot, master, mSelected)))
        return mSelected;

    // Defend self: the first thing actively attacking us.
    for (Unit* attacker : bot->getAttackers())
        if (attacker && attacker->IsAlive() && bot->IsValidAttackTarget(attacker))
            return attacker;

    if (Unit* combatTarget = bot->GetCombatManager().GetAnyTarget())
        if (combatTarget->IsAlive() && bot->IsValidAttackTarget(combatTarget))
            return combatTarget;

    return nullptr;
}

void BotMgr::EnsureGrouped(Player* bot, Player* master)
{
    if (bot->GetGroup())
        return;   // already in a party (the master's) -- nothing to do.

    Group* group = master->GetGroup();
    if (!group)
    {
        // Master is solo: form a party with the master as leader.
        group = new Group();
        if (!group->Create(master))
        {
            delete group;
            return;
        }
        sGroupMgr->AddGroup(group);
    }

    group->AddMember(bot);
}

void BotMgr::RemoveAllBots()
{
    for (auto const& [name, entry] : _bots)
        delete entry.session;
    _bots.clear();
}
