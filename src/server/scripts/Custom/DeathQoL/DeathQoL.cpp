/*
 * Custom: Death QoL — player auto-revive at corpse + the shared corpse-return
 * helper. See docs/superpowers/specs/2026-06-20-death-qol-design.md
 *
 * Bots are intentionally NOT handled here: they aren't registered in
 * World::m_sessions (so GetAllSessions() never returns them) and are revived at
 * their master by BotMgr instead.
 */

#include "DeathQoL.h"
#include "../Bots/BotDeathPolicy.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Position.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"
#include <unordered_map>

namespace Custom::DeathQoL
{
bool ReturnToCorpseAndResurrect(Player* player)
{
    if (!player || player->IsAlive() || !player->HasCorpse())
        return false;

    WorldLocation const corpse = player->GetCorpseLocation();
    player->ResurrectPlayer(1.0f);
    player->SpawnCorpseBones();
    player->TeleportTo(corpse.GetMapId(), corpse.GetPositionX(), corpse.GetPositionY(),
        corpse.GetPositionZ(), corpse.GetOrientation());
    return true;
}
}

namespace
{
// Each interval, scan in-world players and auto-revive the dead at their corpse
// once Custom.PlayerAutoReviveAtCorpse and the configured delay are satisfied.
class deathqol_worldscript : public WorldScript
{
public:
    deathqol_worldscript() : WorldScript("deathqol_worldscript") { }

    void OnUpdate(uint32 diff) override
    {
        if (!sWorld->getBoolConfig(CONFIG_CUSTOM_PLAYER_AUTO_REVIVE_AT_CORPSE))
        {
            _deadMs.clear();
            return;
        }

        _checkTimer += diff;
        if (_checkTimer < CHECK_INTERVAL_MS)
            return;
        uint32 const elapsed = _checkTimer;
        _checkTimer = 0;

        uint32 const delayMs = sWorld->getIntConfig(CONFIG_CUSTOM_PLAYER_AUTO_REVIVE_DELAY_MS);

        for (auto const& [accountId, session] : sWorld->GetAllSessions())
        {
            Player* player = session ? session->GetPlayer() : nullptr;
            if (!player || !player->IsInWorld())
                continue;

            ObjectGuid const guid = player->GetGUID();
            if (player->IsAlive())
            {
                _deadMs.erase(guid);
                continue;
            }

            uint32& deadMs = _deadMs[guid];   // inserts 0 the first tick we see them dead
            deadMs += elapsed;

            bool const bgOrArena = player->GetMap() && player->GetMap()->IsBattlegroundOrArena();
            // enabled/in-world/dead already established above; the helper still owns
            // the bg/arena exclusion and the delay comparison.
            if (Bots::DeathPolicy::ShouldPlayerAutoRevive(true, true, true, bgOrArena, deadMs, delayMs))
            {
                if (Custom::DeathQoL::ReturnToCorpseAndResurrect(player))
                    _deadMs.erase(guid);
            }
        }
    }

private:
    static constexpr uint32 CHECK_INTERVAL_MS = 1000;
    uint32 _checkTimer = 0;
    std::unordered_map<ObjectGuid, uint32> _deadMs;
};
}

void AddSC_deathqol()
{
    new deathqol_worldscript();
}
