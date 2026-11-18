# wow-tofuworld-no-shared-exp

Tofu Worldview module (installs as `mod-tofu-group-xp`): no XP split in groups.

Every grouped member who gets kill XP at all gets the XP they would get killing the mob alone. No split between members, no group bonus, and no halving when a member finds the mob gray. For mobs 5 or more levels above a member, the level gap is extrapolated past the stock +4 cap (`TofuGroupXp.Extrapolate`).

Unchanged: solo kills, battlegrounds, PvP, dead members, members above the highest level for whom the mob is not gray, pets (half in a group), rested XP, and XP auras. Other modules' `OnPlayerGiveXP` rules run afterwards on the result.

## How

`PlayerScript::OnPlayerRewardKillRewarder` runs for each rewarded group member just before the core computes the member's XP. The module recomputes the core's reference member (`KillRewarder::_InitGroupData`) and sets the member's rate to solo XP divided by the reference XP, doubled when the core would halve it. No core edit.

Maintenance: the module copies the reference-member and halving rules of `KillRewarder.cpp` (core 32fd347). Re-check them at every core update.

## Install

1. Place the repo at `modules/mod-tofu-group-xp` (the folder name sets the loader function `Addmod_tofu_group_xpScripts`).
2. Re-run CMake and build.
3. Copy `conf/mod_tofu_group_xp.conf.dist` to the server's `configs/modules` as `mod_tofu_group_xp.conf`.

## Check

Group a level 1 with a level 80 and kill a normal level 80 mob in Northrend: the level 1 gains about 2,896 XP at `Rate.XP.Kill = 1` (before other modules' multipliers), instead of about 12.
