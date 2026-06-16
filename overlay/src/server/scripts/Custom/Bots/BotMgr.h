/*
 * Player-bot overlay for TrinityCore (master) - Milestone 1.
 *
 * BotMgr owns "headless" WorldSessions: real WorldSession objects constructed
 * with a null WorldSocket. Because WorldSession::SendPacket() null-checks the
 * socket, the entire login packet flow becomes a harmless no-op, letting us
 * load a genuine Player into the world with no connected client.
 *
 * M1 scope: spawn/despawn a real character as an idle bot. No AI yet.
 */

#ifndef TRINITYCORE_BOTS_BOTMGR_H
#define TRINITYCORE_BOTS_BOTMGR_H

#include "Define.h"
#include <string>
#include <unordered_map>

class WorldSession;

class BotMgr
{
public:
    static BotMgr* instance();

    // Spawn the named character into the world as a headless bot.
    // Returns false and fills `error` on failure.
    bool AddBot(std::string const& characterName, std::string& error);

    // Despawn (and save) a previously spawned bot by character name.
    bool RemoveBot(std::string const& characterName, std::string& error);

    // Pump every active bot session once per world tick. This is what lets the
    // asynchronous login query-holder callback fire (WorldSession::Update ->
    // ProcessQueryCallbacks), and keeps the session alive thereafter.
    void Update(uint32 diff);

    // Despawn all bots (used on world shutdown so characters are saved).
    void RemoveAllBots();

    std::size_t GetBotCount() const { return _bots.size(); }

private:
    BotMgr() = default;
    ~BotMgr() = default;
    BotMgr(BotMgr const&) = delete;
    BotMgr& operator=(BotMgr const&) = delete;

    // lowercased character name -> owning headless session
    std::unordered_map<std::string, WorldSession*> _bots;
};

#define sBotMgr BotMgr::instance()

#endif // TRINITYCORE_BOTS_BOTMGR_H
