/*
 * Player-bot support for TrinityCore (master) - Milestone 1.
 * See BotMgr.h for the design rationale.
 */

#include "BotMgr.h"
#include "CharacterCache.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Realm/ClientBuildInfo.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"
#include "WorldSocket.h"
#include <algorithm>
#include <cctype>
#include <memory>

BotMgr* BotMgr::instance()
{
    static BotMgr instance;
    return &instance;
}

namespace
{
    std::string ToLower(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return str;
    }
}

bool BotMgr::AddBot(std::string const& characterName, std::string& error)
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
    session->SetBot(true);

    _bots[key] = session;

    // Reuse the real async login machinery (LoginQueryHolder -> HandlePlayerLogin).
    // The holder callback fires from this session's Update() in BotMgr::Update().
    session->LoadBotCharacter(guid);

    TC_LOG_INFO("bots", "BotMgr: spawning bot for character '{}' (account {}).", characterName, accountId);
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

    WorldSession* session = itr->second;
    _bots.erase(itr);

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

    for (auto const& [name, session] : _bots)
    {
        // MapSessionFilter::ProcessUnsafe() == false, which skips the logout /
        // socket-cleanup block in WorldSession::Update() that would otherwise
        // return false (and signal deletion) for our socket-less session.
        MapSessionFilter filter(session);
        session->Update(diff, filter);
    }
}

void BotMgr::RemoveAllBots()
{
    for (auto const& [name, session] : _bots)
        delete session;
    _bots.clear();
}
