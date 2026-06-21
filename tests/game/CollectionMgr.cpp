#include "tc_catch2.h"

#include "CollectionMgr.h"

TEST_CASE("Bulk appearance grants suppress transmog set reward quests during login unlocks", "[CollectionMgr]")
{
    REQUIRE(CollectionMgr::ShouldGrantAppearanceSetRewards(AppearanceGrantSource::Normal));
    REQUIRE_FALSE(CollectionMgr::ShouldGrantAppearanceSetRewards(AppearanceGrantSource::BulkLoginGrant));
}
