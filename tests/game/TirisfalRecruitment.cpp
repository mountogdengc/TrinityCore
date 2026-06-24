#include "tc_catch2.h"

#include "QuestDef.h"
#include "tirisfal_recruitment.h"

TEST_CASE("Tirisfal recruitment helper recognizes Darnell quest states", "[TirisfalRecruitment]")
{
    REQUIRE(Tirisfal::Recruitment::IsDarnellQuest(25089));
    REQUIRE(Tirisfal::Recruitment::IsDarnellQuest(26800));
    REQUIRE_FALSE(Tirisfal::Recruitment::IsDarnellQuest(24971));
}

TEST_CASE("Tirisfal recruitment helper resummons Darnell only for active quest progress", "[TirisfalRecruitment]")
{
    REQUIRE(Tirisfal::Recruitment::ShouldEnsureDarnell(QUEST_STATUS_INCOMPLETE));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldEnsureDarnell(QUEST_STATUS_NONE));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldEnsureDarnell(QUEST_STATUS_COMPLETE));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldEnsureDarnell(QUEST_STATUS_REWARDED));
}

TEST_CASE("Tirisfal recruitment helper cleans up Darnell only when the quest is gone", "[TirisfalRecruitment]")
{
    REQUIRE(Tirisfal::Recruitment::ShouldCleanupDarnell(QUEST_STATUS_NONE));
    REQUIRE(Tirisfal::Recruitment::ShouldCleanupDarnell(QUEST_STATUS_REWARDED));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldCleanupDarnell(QUEST_STATUS_INCOMPLETE));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldCleanupDarnell(QUEST_STATUS_COMPLETE));
}

TEST_CASE("Tirisfal recruitment helper grants corpse credit only once per owner while quest is active", "[TirisfalRecruitment]")
{
    REQUIRE(Tirisfal::Recruitment::CanGrantRecruitmentCredit(QUEST_STATUS_INCOMPLETE, true, false));
    REQUIRE_FALSE(Tirisfal::Recruitment::CanGrantRecruitmentCredit(QUEST_STATUS_INCOMPLETE, false, false));
    REQUIRE_FALSE(Tirisfal::Recruitment::CanGrantRecruitmentCredit(QUEST_STATUS_INCOMPLETE, true, true));
    REQUIRE_FALSE(Tirisfal::Recruitment::CanGrantRecruitmentCredit(QUEST_STATUS_COMPLETE, true, false));
}

TEST_CASE("Tirisfal recruitment helper prioritizes Darnell assist targets like a quest companion", "[TirisfalRecruitment]")
{
    using Tirisfal::Recruitment::DarnellAssistSource;

    REQUIRE(Tirisfal::Recruitment::SelectDarnellAssistSource(true, true, true, true, true) == DarnellAssistSource::MasterVictim);
    REQUIRE(Tirisfal::Recruitment::SelectDarnellAssistSource(false, true, true, true, true) == DarnellAssistSource::MasterSelectedTarget);
    REQUIRE(Tirisfal::Recruitment::SelectDarnellAssistSource(false, false, true, true, true) == DarnellAssistSource::MasterAttacker);
    REQUIRE(Tirisfal::Recruitment::SelectDarnellAssistSource(false, false, false, false, true) == DarnellAssistSource::SelfAttacker);
    REQUIRE(Tirisfal::Recruitment::SelectDarnellAssistSource(false, false, true, false, false) == DarnellAssistSource::None);
}

TEST_CASE("Tirisfal recruitment helper only scans and collects corpses while Recruitment is active", "[TirisfalRecruitment]")
{
    REQUIRE(Tirisfal::Recruitment::ShouldScanRecruitmentCorpses(QUEST_STATUS_INCOMPLETE));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldScanRecruitmentCorpses(QUEST_STATUS_COMPLETE));
    REQUIRE_FALSE(Tirisfal::Recruitment::ShouldScanRecruitmentCorpses(QUEST_STATUS_REWARDED));

    REQUIRE(Tirisfal::Recruitment::CanCollectRecruitmentCorpse(QUEST_STATUS_INCOMPLETE, false));
    REQUIRE_FALSE(Tirisfal::Recruitment::CanCollectRecruitmentCorpse(QUEST_STATUS_INCOMPLETE, true));
    REQUIRE_FALSE(Tirisfal::Recruitment::CanCollectRecruitmentCorpse(QUEST_STATUS_COMPLETE, false));
}
