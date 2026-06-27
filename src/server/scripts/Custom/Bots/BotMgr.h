/*
 * Player-bot support for TrinityCore (master).
 *
 *   M1: spawn/despawn a real character as a headless bot (idle, no AI).
 *   M2: the bot follows a "master" player, zones/teleports with them, and stays
 *       alive across map changes.
 *
 * BotMgr owns "headless" WorldSessions: real WorldSession objects constructed
 * with a null WorldSocket. Because WorldSession::SendPacket() null-checks the
 * socket, the entire packet flow becomes a harmless no-op, letting us load a
 * genuine Player into the world with no connected client.
 */

#ifndef TRINITYCORE_BOTS_BOTMGR_H
#define TRINITYCORE_BOTS_BOTMGR_H

#include "BotFormationPolicy.h"
#include "Define.h"
#include "ObjectGuid.h"
#include <string>
#include <unordered_map>

class Player;
class Unit;
class WorldSession;

class BotMgr
{
public:
    static BotMgr* instance();

    bool AddBot(ObjectGuid characterGuid, ObjectGuid master, std::string& error);

    // Spawn the named character into the world as a headless bot. If `master` is
    // not empty (M2), the bot follows that player. Returns false + fills `error`.
    bool AddBot(std::string const& characterName, ObjectGuid master, std::string& error);

    // Despawn (and save) a previously spawned bot by character name.
    bool RemoveBot(std::string const& characterName, std::string& error);

    // M2: set the player a bot follows; an empty guid clears it (hold position).
    bool SetMaster(std::string const& characterName, ObjectGuid master, std::string& error);

    // Pump every active bot session once per world tick (drives the async login
    // callback and keeps the socket-less session alive), then run follow logic.
    void Update(uint32 diff);

    // Despawn all bots (used on world shutdown so characters are saved).
    void RemoveAllBots();

    std::size_t GetBotCount() const { return _bots.size(); }

    // Per-master follow formation (in-memory; defaults to Wedge).
    BotFormation GetFormation(ObjectGuid master) const;
    void SetFormation(ObjectGuid master, BotFormation preset);

private:
    BotMgr() = default;
    ~BotMgr() = default;
    BotMgr(BotMgr const&) = delete;
    BotMgr& operator=(BotMgr const&) = delete;

    struct BotEntry
    {
        WorldSession* session;
        ObjectGuid    master;                // empty => idle (M1 behaviour: hold position)
        uint32        holdTimer = 0;         // M3: ms left to linger after a fight before re-following
        uint32        deadTimer = 0;         // M4: ms the bot has been dead (drives auto-revive)
        uint32        staleCombatTimer = 0;  // M4: ms the current victim has looked invalid (kept until BOT_STALE_COMBAT_MS)
        uint8         formationSlot = 0;     // per-bot index -> distinct combat chase angle (anti-stacking); follow now uses the dynamic squad index
        ObjectGuid    combatTarget;          // current chase target; drives re-issue of MoveChase on a switch
        uint32        rangedAutoSpellId = 0; // cached autorepeat ranged spell (Auto Shot / wand Shoot); 0 = none
        bool          rangedAutoChecked = false; // true once we've scanned this bot's spells for the above
        uint32        petEntry = 0;          // cached chosen tameable-beast entry (0 = not yet picked)
        uint32        petDeadTimer = 0;      // ms the hunter pet has been dead (drives revive delay)
        uint32        formationKey = 0xFFFFFFFF; // last-applied (preset|index|count) packing; re-issue follow on change
    };

    // M2: make every bot with a master chase / zone with that player.
    void UpdateFollow();

    // M3: put the bot in the master's party (forming one if the master is solo),
    // so they share quest/loot context and -- crucially -- the same dungeon/raid
    // instance when zoning.
    void EnsureGrouped(Player* bot, Player* master);

    // M3: pick who the bot should fight -- the master's target while they're in
    // combat (assist), else whatever is attacking the bot (defend self). Returns
    // nullptr when there's nothing to fight.
    Unit* SelectAssistTarget(Player* bot, Player* master);

    // Find this bot's autorepeat ranged spell (Auto Shot for a Hunter, wand Shoot for a
    // wand-equipped caster), or 0 if it has no usable ranged weapon / no such spell.
    uint32 FindRangedAutoAttackSpell(Player* bot);

    // Hunter pets: reconcile pet state (summon / revive / level-sync) each follow tick.
    void EnsureHunterPet(Player* bot, BotEntry& entry);
    // Summon a HUNTER_PET of `entry` for the bot at the bot's level (the tame sequence).
    void SummonBotPet(Player* bot, uint32 entry);
    // Nearest tameable beast to the bot, or BOT_DEFAULT_PET_ENTRY if none in range.
    uint32 PickTameableBeastEntry(Player* bot);

    // lowercased character name -> bot
    std::unordered_map<std::string, BotEntry> _bots;
    std::unordered_map<ObjectGuid, BotFormation> _formations;   // master guid -> chosen follow formation
    uint32 _followTimer = 0;
    uint8  _nextFormationSlot = 0;   // hands out BotEntry::formationSlot on AddBot
};

#define sBotMgr BotMgr::instance()

#endif // TRINITYCORE_BOTS_BOTMGR_H
