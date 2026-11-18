/*
 * mod-tofu-group-xp (Tofu Worldview): no XP split in groups.
 *
 * Stock rule (KillRewarder.cpp at core 32fd347): in a group the core computes one XP value for the highest-level alive member in range for whom the mob is not gray (the reference member), then gives each member value * groupRate * memberLevel / sumOfAliveLevels, halved plus 1 when some member finds the mob gray. BaseGain caps the level gap at +4 for higher mobs.
 *
 * This module: each grouped member who would get kill XP at all gets the XP they would get killing the mob alone. No split, no group bonus, and no halving for a gray member. For a mob 5 or more levels above the member, the level gap is extrapolated past the +4 cap (TofuGroupXp.Extrapolate).
 *
 * How: OnPlayerRewardKillRewarder runs for each rewarded member just before the core computes the member's XP as referenceXP * rate (or referenceXP * rate / 2 + 1 when some member finds the mob gray). The hook recomputes the reference member with the core's rules and sets rate = soloXP / referenceXP (doubled in the halved case). The core's low-health XP factor is in both values, so it cancels. Members the core gives 0 (dead, above the reference level, gray mob) are left alone, as are solo kills (the core ignores rate without a group), battlegrounds, and PvP. Every other module's OnPlayerGiveXP (progression caps, individual-xp, challenge modes, playerbots' bot XP rate) runs afterwards on the result.
 *
 * Maintenance: this copies the reference-member and halving rules of KillRewarder. Re-check them at every core update.
 */

#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "Formulas.h"
#include "Group.h"
#include "KillRewarder.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace
{
    bool enabled = true;
    bool extrapolate = true;

    uint8 LevelForXp(Player const* player)
    {
        uint8 level = player->GetLevel();
        sScriptMgr->OnPlayerBeforeGetLevelForXPGain(player, level);
        return level;
    }

    // BaseGain for a mob at or above the player's level, without the +4 cap (same formula as Acore::XP::BaseGain).
    uint32 UncappedHigherBaseGain(uint8 playerLevel, uint8 mobLevel, ContentLevels content)
    {
        uint32 baseExp = 45;
        if (content == CONTENT_61_70)
            baseExp = 235;
        else if (content == CONTENT_71_80)
            baseExp = 580;
        uint32 const levelDiff = uint32(mobLevel) - uint32(playerLevel);
        return ((uint32(playerLevel) * 5 + baseExp) * (20 + levelDiff) / 10 + 1) / 2;
    }

    // What this member would get killing the mob alone, before the core's low-health factor (which the reference value also carries, so it cancels in the ratio).
    float SoloXp(Player* member, Unit* victim)
    {
        float xp = float(Acore::XP::Gain(member, victim, false));
        if (xp <= 0.0f || !extrapolate)
            return xp;

        uint8 const playerLevel = LevelForXp(member);
        uint8 const mobLevel = victim->GetLevel();
        if (mobLevel < playerLevel + 5)
            return xp;

        ContentLevels const content = GetContentLevelsForMapAndZone(victim->GetMapId(), victim->GetZoneId());
        uint32 const capped = Acore::XP::BaseGain(playerLevel, mobLevel, content);
        if (!capped)
            return xp;
        return xp * float(UncappedHigherBaseGain(playerLevel, mobLevel, content)) / float(capped);
    }
}

class TofuGroupXp_WorldScript : public WorldScript
{
public:
    TofuGroupXp_WorldScript() : WorldScript("TofuGroupXp_WorldScript") {}

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        enabled = sConfigMgr->GetOption<bool>("TofuGroupXp.Enable", true);
        extrapolate = sConfigMgr->GetOption<bool>("TofuGroupXp.Extrapolate", true);
    }
};

class TofuGroupXp_PlayerScript : public PlayerScript
{
public:
    TofuGroupXp_PlayerScript() : PlayerScript("TofuGroupXp_PlayerScript", { PLAYERHOOK_ON_REWARD_KILL_REWARDER }) {}

    void OnPlayerRewardKillRewarder(Player* player, KillRewarder* rewarder, bool /*isDungeon*/, float& rate) override
    {
        if (!enabled || !player || !rewarder)
            return;

        Player* killer = rewarder->GetKiller();
        Unit* victim = rewarder->GetVictim();
        if (!killer || !victim)
            return;

        // PvE kills of creatures only: no XP for PvP, and battleground player kills use their own XP path.
        Creature* creature = victim->ToCreature();
        if (!creature || creature->GetCharmerOrOwnerGUID().IsPlayer())
            return;

        Group* group = killer->GetGroup();
        if (!group)
            return; // solo kill: the core ignores rate, the stock rule stays

        // The core's reference member and gray flag (KillRewarder::_InitGroupData)
        uint8 maxLevel = 0;
        uint8 referenceLevel = 0;
        Player* reference = nullptr;
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member || !(member == killer || member->IsAtGroupRewardDistance(victim)) || !member->IsAlive())
                continue;
            uint8 const level = LevelForXp(member);
            if (level > maxLevel)
                maxLevel = level;
            if (victim->GetLevel() > Acore::XP::GetGrayLevel(level) && (!reference || referenceLevel < level))
            {
                reference = member;
                referenceLevel = level;
            }
        }
        if (!reference)
            return; // gray for everyone: the core gives 0

        // Members the core gives 0 stay at 0 (KillRewarder::_RewardXP)
        if (!player->IsAlive() || LevelForXp(player) > referenceLevel)
            return;

        float const referenceXp = float(Acore::XP::Gain(reference, victim, false));
        if (referenceXp <= 0.0f)
            return;

        float newRate = SoloXp(player, victim) / referenceXp;
        bool const fullXp = maxLevel == referenceLevel;
        if (!fullXp)
            newRate *= 2.0f; // the core halves (xp * rate / 2 + 1); undo the halving
        rate = newRate;
    }
};

void AddSC_tofu_group_xp()
{
    new TofuGroupXp_WorldScript();
    new TofuGroupXp_PlayerScript();
}
