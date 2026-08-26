# Moba Prototype

I’ve wanted to make a 3D multiplayer MOBA for a long time. I messed around in Blueprints on and off for a while. This is a C++ multiplayer prototype, built with GAS.

## Preview

![Gameplay](gameplay.gif)

## Play

### Packaged (Win64)

Download the archive and extract the zip.

- [Standalone](https://github.com/debsamanta5571-dot/moba-prototype/releases/latest/download/MobaPrototype-Standalone-Win64.zip)
- [Client](https://github.com/debsamanta5571-dot/moba-prototype/releases/latest/download/MobaPrototype-Client-Win64.zip)
- [Dedicated server](https://github.com/debsamanta5571-dot/moba-prototype/releases/latest/download/MobaPrototype-Server-Win64.zip)

**Standalone** is `MobaProject.exe`. You can play on the same network using Lan or you can use a VPN like Radmin to connect over the internet. 

**Client** is `MobaProjectClient.exe`. This build can join servers but can't host.

**Dedicated server** is `StartServer.bat`. Wait until the window prints **SERVER READY**, then join from a client. It listens on port **7777** with no local player.

### Controls

| | |
|---|---|
| WASD | Move |
| Mouse | Look and Aim |
| Abilites| LMB, Q, LShift, E| 
| Shop | B, Can only work in your spawn area or when your dead |
| Ability Infortmation| TAB|
| Inventory| I |
| Settings |Backspace or Escape|

### Game features

- 2 heroes: Brawler and Mage
- Minion waves that walk and fight
- 2 towers, destroy the enemy tower to win
- Shop in spawn: Damage, Energy, CDR, Health, Resist, Move Speed, Gold Regen
- 4 unique abilities per hero
- Melee sweep, skillshot, ground slam, beam, and dash
- Last-hit gold on minions
- Health and energy regen in spawn
- Status effects: slow, stun, haste, and heal
- Respawn after death
- Towers prefer minions, then stick on a hero they are already shooting

## Architecture

### Extending the ability system

If you want another ability of a type that already exists, duplicate a `BP_GA_*` and change the numbers, montage, and effects. Trace, projectile, ground AoE, beam, and dash are already implemented; assign the new class to a slot on the hero.

A new delivery type is a subclass of `UMobaGameplayAbility` with `PrepareCast` / `OnCastNotify` implemented. That keeps prediction, cooldown tags, energy, plant, and the montage notify. Starting from `UGameplayAbility` skips all of that.

A new hero is a Blueprint child of `AMobaBaseCharacter`. Fill `AbilitySlots` on that Blueprint; empty slots copy from the parent CDO. Mesh, hat, and the shop catalog live there too.

On-hit extras are `FMobaEffectSpec` rows: slow, stun, heal, and haste. Damage still goes through `UMobaCombatLibrary::ApplyMobaDamage`, which returns false on simulated copies and friendly fire.

A lasting stat is a replicated field on `UMobaAttributeSet`. Initialize it on the hero CDO, then either read it in the combat library (damage, resist) or add an `EMobaShopStat` case if the shop should sell it. Short crowd control belongs on `UMobaStatusComponent`.

Two heroes are still hardcoded in `GetHeroChoiceCount` / `GetHeroClassAt` (`BP_Brawler`, `BP_Mage`), teams are 1 and 2, and montage notifies are `Ability.1` through `Ability.4`. A fifth slot needs a new tag and a HUD slot.

### Ability core

`UMobaGameplayAbility` is `InstancedPerActor` and `LocalPredicted`. GAS input triggers are cleared, so Enhanced Input on the pawn is what presses the spec. `ActivateAbility` does not call `Super`, because Super would run an empty Blueprint Activate graph (`bHasBlueprintActivate`) and end the ability immediately.

A cast goes `PrepareCast`, then `CommitAbility`, then a cooldown tag and a server energy spend, then an optional plant (walk-speed scale while casting), then the montage. `UAnimNotify_AbilityEvent` fires `Ability.1` through `Ability.4`, and `UAbilityTask_WaitGameplayEvent` waits for that tag. If the montage ends with no notify, the hit still fires. A listen host plays the montage locally and again from replication; `bCastMontageDone` keeps BlendOut and Interrupted from ending the ability twice.

Cooldown is a loose tag on the Ability System Component rather than a timer on the ability object, because `CanActivateAbility` can run on the CDO, which has no world. `ResolveCooldownTag` maps the class back to the hero slot. Cooldown reduction scales duration, clamped to 0–0.8. Energy is spent only on authority; predicting the spend rubber-bands the bar.

### Ability types

`UGA_MobaTrace` is a yaw-flattened sphere sweep. Prediction plays the swing, and only authority writes Health. Pitch is ignored, so the sweep stays level when you look up.

`UGA_MobaProjectile` uses camera direction rather than actor yaw, so orbs can lob. Authority spawns the damaging bolt. The owning client spawns a cosmetic copy unless it is also the listen host, which already has the authority actor. The projectile does not replicate (`bReplicates = false`).

`UGA_MobaGroundAoE` is hold to aim and release to confirm. The client sends `ServerConfirmGroundTarget` with the point it traced, and the server does not re-trace the camera. Damage is an expanding wave on the ability instance; `InstancedPerActor` is why that wave can outlive `EndAbility`.

`UGA_MobaBeam` leaves `bEndOnMontage` off and ticks damage along a smoothed aim. `UMobaBeamComponent` replicates start, direction, and range with `COND_SkipOwner`, and `State.Beaming` blocks retrigger.

`UGA_MobaDash` sends this-frame WASD with the activate (`bSendMoveDirection`); without that, the server dashes the wrong way. Motion is `UAbilityTask_ApplyRootMotionConstantForce` through Character Movement, so it is included in the SavedMove. `SetActorLocation` on the client rubber-bands.

Hits go through `UMobaCombatLibrary::ApplyMobaDamage`, which ignores friends (`MobaIsEnemy`) and returns false unless the target has authority. Self-effects such as lifesteal apply on the first successful target only.

### Attributes and abilities

Heroes, minions, and towers all share one `UMobaAttributeSet` on the ASC (`GetFromActor`). Fields replicate with `REPNOTIFY_Always`: health, energy, gold, their regen, damage modifier, cooldown reduction, resist, and move speed.

Abilities only hard-wire `EnergyCost` and `Cooldown` on the CDO. Kit damage is a raw number on the ability: trace 35, bolt 25, and so on.

**CanActivate** checks `HasEnergy(EnergyCost)`. Dead and stunned tags also block.

**Commit** calls `StartCooldown`, which scales duration by `1 - CDR` (CDR clamped to 0.8). `SpendEnergy` runs only on authority.

**Hit** is `ApplyMobaDamage(Target, Amount, Instigator)`. Amount is the ability's Damage. Incoming is multiplied by the attacker's `DamageModifier`, then by `1 - target.DamageResistance` (resist clamped to 0.9). The HUD ability text uses the same formula.

**On-hit specs** are `FMobaEffectSpec` on the ability. Heal writes Health, clamped to max. Slow, stun, and haste go to `UMobaStatusComponent`.

The shop writes the same set. `EMobaShopStat` maps Damage to `DamageModifier`, Health to max and current, Energy to max and current, and so on. Repeat buys cost `base * 1.2^n`. `ServerBuyOffer` only runs in the fountain or while dead, and it re-checks gold and catalog. One +10% damage buy scales every trace, bolt, slam, and beam tick without touching the ability Blueprints.

Minions and towers initialize a subset: health, damage modifier, resist, and gold on kill. A tower hitting a minion ignores modifier and resist and deals a percent of max HP, so wave clear stays even as heroes buy. Last-hit gold uses a kill-credit window on whoever damaged the victim.

### Effects

The ability CDO has an `Effects` array of `FMobaEffectSpec`. Each spec is a type, a target (`HitActor` or `Self`), a magnitude, and a duration. You set those rows on the Blueprint; `ApplyMobaEffects` applies them.

`ApplyAbilityHit` applies damage first, and specs run only if that write succeeds, so a friend or a simulated copy never gets the effect. `HitActor` specs go to the victim and `Self` specs go to the caster. Sweeps, slam waves, and beam ticks apply self effects on the first successful hit only; otherwise a self-heal would fire once per body in the cone.

Heal adds Health, clamped to max. Slow, stun, and haste go through `UMobaStatusComponent` on heroes and minions. The component replicates the flags, the speed multipliers, and the end times, and it sets loose tags (`State.Stunned`, `State.Slowed`, `State.Hasted`) so a stunned pawn fails `CanActivateAbility`. Stun also cancels beam and a held ground aim. Towers have no status component.

Walk speed is `base * slow * haste * plant`. Haste multiplies with slow and plant rather than replacing the base. Slows stack with each other as `(1 - amount)`; values over 1 are treated as percent, so `20` is 20%. Stun skips the product and disables movement until the timer expires.

To add a slow to an existing bolt, add a row to `Effects`. A new effect type is an `EMobaEffectType` case and a branch in `ApplySpec`. If the effect lasts a few seconds, give it a replicated field and a timer on the status component. If it should last the match, put it on `UMobaAttributeSet`, and on `EMobaShopStat` if the shop should sell it.

### Networking

Unreal replicates the pawn, Character Movement, and attributes. Casts use GAS `LocalPredicted`. Custom Server, NetMulticast, and Client RPCs cover the rest: an aim point, this-frame WASD, a shop buy, lobby choices, and cosmetics that would otherwise play twice on the owner.

The owning client activates immediately (montage, cooldown tag, cosmetic bolt, dash root motion) and the server activates the same spec. `ApplyMobaDamage` still requires the target to have authority. Energy is spent only when `HasAuthority(&CurrentActivationInfo)` is true; predicting that spend rubber-bands the energy bar.

Ground slam sends `ServerConfirmGroundTarget` with the client's floor point, which the server stores before calling `TryActivateAbilityByClass` instead of re-tracing the camera. Dash writes WASD locally and, on a remote client, calls `ServerSetPendingAbilityDirection` before activate. Beam damage ticks on authority only; start, direction, range, and the ground-blast cue replicate with `COND_SkipOwner`.

Lobby state lives on `AMobaPlayerState` (team, hero, loadout, map-loaded). `ServerStartMatchFromLobby` checks `bLobbyLeader`: the UI hides Start for joiners, and the RPC still enforces the flag. Shop `ServerBuyOffer` re-runs `CanBuy` (gold, range, catalog) before it writes the attribute set. `PurchasedOffers` and `bInShopRange` replicate. Victory is `MulticastMatchOver` from `AMobaVictoryManager`. Attributes use `DOREPLIFETIME_CONDITION_NOTIFY` with `REPNOTIFY_Always`. Stun, slow, and haste replicate from `UMobaStatusComponent`.

Montage, slam, fire-ring, ground-blast, and skillshot cosmetics are NetMulticast and return early on `IsLocallyControlled()`. SFX multicast skips authority and owner, since those already played locally. Damage and gold numbers are Client RPCs on the pawn that dealt, took, or earned them. VFX and SFX are Unreliable; montage, blast, and lobby RPCs are Reliable.

Simulated proxies start the cooldown tag late by RTT. `UMobaNetLibrary::CompensateCooldown` subtracts one-way ping (half of `PlayerState` ping, clamped to 0.2s), and `CooldownSanity` replicates remaining time for late joiners. Lobby sliders write `FPacketSimulationSettings` on the client net driver (`PktLagMin` / `PktIncomingLagMin`).


Made with C++ and Unreal 5.8.
