# Moba Prototype

I've always wanted to make a 3D multiplayer MOBA. I spent years poking at it in Blueprint; this is a C++ prototype of that idea, built with friends in mind as the other seats at the table.

Two heroes, one lane, a shop, minions, and towers. You take a side, spend gold, and try to knock the other team's building down. It is a gameplay and netcode slice, not a shipped game.

## Preview

![Gameplay](gameplay.gif)

## Play

Open `MobaProject/MobaProject.uproject` in a **source-built Unreal 5.8** editor (this project is not wired to the Epic Games Launcher). Play `Moba/Maps/MobaMenu`.

- **Host** starts a listen lobby. Pick **Brawler** or **Mage**, pick a team, then **Start**.
- A second PIE window or packaged client **Join**s `127.0.0.1:7777`.
- On a dedicated server, the first joiner is lobby leader.

| | |
|---|---|
| WASD | Move |
| Mouse | Look |
| Ability slots (HUD keys, plus Q / E) | Cast. Hold to aim ground abilities, release to fire |
| Shop | Open in your fountain, or while dead |
| Inventory / descriptions / settings | Overlay panels |

Ability VFX are `DrawDebug` shapes on purpose. Placeholder presentation, not Niagara.

Packaged Win64 builds (standalone host, client-only, dedicated `StartServer.bat` on port **7777**) are local packages, not in this repo. The client exe cannot host.

## Architecture

Unreal is the machine: pawn, Character Movement, replication, widgets, cooking. The *match* is a small set of C++ types that sit on that machine. Blueprint does not decide whether a swing connected.

```
Session / front-end (host, join, lobby, travel)
        │
   GameMode + PlayerState (teams, wait-for-players, loadout)
        │
   Hero pawn (GAS avatar, input, death)
        ├── ability parent + typed children
        ├── combat library (the only Health write)
        ├── attribute set + status + shop
        └── minion AI controller / towers
```

**What Unreal owns.** Possession, `UCharacterMovementComponent`, actor replication, Enhanced Input, UMG, `ServerTravel`, the three cook targets (game / client / server).

**What the game owns.** Predicted casts, who is an enemy, who may spend gold, when the match unlocks, how a minion picks a target. Those rules are C++. A designer-facing Blueprint fills *data* on top: which four classes sit in the slots, montage, damage, icon.

That split is why a new kit is a subclass, and a new hero is mostly a Blueprint.

### Extending it

Easy, and that is the point of the types:

- **New ability of an existing kind** — duplicate a `BP_GA_*`, change numbers / montage / effects. Trace, projectile, ground AoE, beam, and dash already exist. Put the class in a slot on the hero.
- **New delivery type** — subclass `UMobaGameplayAbility`, implement `PrepareCast` / `OnCastNotify`. You get prediction, cooldown tags, energy, plant, and montage notify for free. Do not start from `UGameplayAbility`.
- **New hero** — Blueprint child of `AMobaBaseCharacter`, fill `AbilitySlots`. Slots copy from the parent CDO if the child left them empty. Mesh, hat, shop catalog live on that BP.
- **New on-hit behavior** — add an `FMobaEffectSpec` (slow, stun, heal, haste). Damage still goes through `UMobaCombatLibrary::ApplyMobaDamage`, which refuses simulated copies and friendly fire.
- **New persistent stat** — add a replicated field on `UMobaAttributeSet`, init it on the hero CDO, then either read it in the combat library (like damage / resist) or add an `EMobaShopStat` case so the shop can buy it. Temporary CC is not an attribute; it is `UMobaStatusComponent`.

Not a framework. Two heroes are hardcoded in `GetHeroChoiceCount` / `GetHeroClassAt` (`BP_Brawler`, `BP_Mage`). Teams are 1 and 2. Montage notifies are `Ability.1`–`Ability.4`, so a fifth slot is a tag and a HUD slot, not a one-click. Those are the seams.

### Ability core

`UMobaGameplayAbility` is `InstancedPerActor` and `LocalPredicted`. GAS input triggers are cleared; Enhanced Input on the pawn presses the spec. `ActivateAbility` does not call `Super` — empty Blueprint Activate graphs set `bHasBlueprintActivate` and would end the ability immediately.

Cast path: `PrepareCast` → `CommitAbility` → cooldown tag + server energy spend → optional plant (walk speed) → montage. `UAnimNotify_AbilityEvent` fires `Ability.1`–`Ability.4`. `UAbilityTask_WaitGameplayEvent` catches that tag. If the montage ends with no notify, the hit still goes out so a missing notify does not eat the cast. Listen host plays the montage locally and again from replication; `bCastMontageDone` gates BlendOut/Interrupted so that double play does not double-end.

Cooldown is a loose tag on the **Ability System Component**, not a timer on the ability object. `CanActivateAbility` can run on the CDO, which has no world. `ResolveCooldownTag` maps the class back to the hero slot. CDR scales the duration (clamped 0–0.8). Energy is a server number; predicting spend rubber-bands the bar.

### Ability types

Children are delivery types, not hero names.

- **`UGA_MobaTrace`** — yaw-flattened sphere sweep. Predicted activate plays the swing; only authority writes Health. Looking up does not throw the sweep into the sky.
- **`UGA_MobaProjectile`** — camera direction, not actor yaw, so orbs can lob. Authority spawns the damaging bolt. The owning client spawns a cosmetic copy. Listen host already has the authority bolt, so it does not spawn a second visual.
- **`UGA_MobaGroundAoE`** — hold to aim, release to confirm. The client sends `ServerConfirmGroundTarget` with the point it traced. The server does not re-trace the camera. Damage is an expanding wave on the ability instance (`InstancedPerActor` survives `EndAbility`), not a tick on the pawn.
- **`UGA_MobaBeam`** — `bEndOnMontage` off. Timers tick damage along a smoothed aim. `UMobaBeamComponent` replicates start/dir/range with `COND_SkipOwner`. `State.Beaming` blocks retrigger.
- **`UGA_MobaDash`** — WASD for that frame is sent with the activate (`bSendMoveDirection`) or the server dashes the wrong way. Motion is `UAbilityTask_ApplyRootMotionConstantForce` through Character Movement so the SavedMove includes it. `SetActorLocation` on the client rubber-bands.

`ApplyAbilityHit` goes through `UMobaCombatLibrary::ApplyMobaDamage`. That function returns false unless the **target** has authority, so a simulated copy never writes Health. Friends are ignored (`MobaIsEnemy`). Self-effects (lifesteal) apply on the first successful target only so a multi-hit does not stack them.

### Attributes and abilities

One `UMobaAttributeSet` on the ASC. Heroes, minions, and towers all use it (`GetFromActor`). Fields replicate with `REPNOTIFY_Always`: health, energy, gold, their regen, damage modifier, CDR, resist, move speed.

Abilities do not hard-wire those fields except two knobs on the ability CDO: `EnergyCost` and `Cooldown`. The rest of the kit is a raw number (trace damage 35, bolt 25, …). The pipeline is:

1. **CanActivate** — `HasEnergy(EnergyCost)`. Dead / stunned tags also block.
2. **Commit** — `StartCooldown` scales duration by `1 - CDR` (CDR clamped to 0.8). `SpendEnergy` runs only on authority so the bar does not rubber-band.
3. **Hit** — `ApplyMobaDamage(Target, Amount, Instigator)`. Amount is the ability’s Damage. Incoming `*= attacker.DamageModifier`, then `*= (1 - target.DamageResistance)` (resist clamped to 0.9). The HUD ability text multiplies the same way, so the number you read is the number that lands.
4. **On-hit specs** — `FMobaEffectSpec` on the ability. Heal writes Health (clamped to max). Slow / stun / haste go to `UMobaStatusComponent`, not the set: slows stack multiplicatively (values over 1 are percent, `20` → 20%), stun also sets `State.Stunned` on the ASC and cancels beam / hold.

Shop writes the **same set**. `EMobaShopStat` maps Damage → `DamageModifier`, Health → max+current, Energy → max+current, and so on. Repeat buys cost `base * 1.2^n`. Fountain overlap (or death) is the only place `ServerBuyOffer` runs. Buy +10% damage once and every trace, bolt, slam, and beam tick scales; you do not touch the ability Blueprints.

Minions and towers init a subset (health, damage modifier, resist, gold-on-kill). Towers hitting minions ignore modifier/resist and deal a percent of **max** HP so wave clear stays even as they level. Last-hit gold uses a kill-credit window on whoever damaged the victim.

A new *lasting* number is an attribute. A new *for a few seconds* number is an effect spec. Mixing those is how a shop item and a slow shot both change a fight without two pipelines.

### Three targets, one module

`MobaProject.Target.cs` (game), `MobaProjectClient.Target.cs` (client), `MobaProjectServer.Target.cs` (server) all load the same `MobaProject` module. Client cannot listen (`IsRunningClientOnly`). Server skips `MoviePlayer` in the Build.cs. `UMobaSessionSubsystem` hosts, joins, and travels. `UMobaFrontEndSubsystem` owns menu, lobby, loading movie, and settings.

Lobby start stamps `WaitPlayers=N` on the arena URL and adds `?listen` only on a listen-server. Dedicated travel omits listen or the arena waits on a session that is not there. `AMobaGameMode` unlocks when that many player states have `ServerNotifyMapLoaded`, or after 20 seconds if someone DC'd in travel. Late joiners get a loadout, then `SpawnLateJoiner`. Hero and team are cached across travel by player id and name keys (`P{id}`, `N{name}`) because PlayerState objects do not survive the map change.

### Net feel

Owning client predicts the cast. Simulated proxies start the cooldown bar late by RTT; `UMobaNetLibrary::CompensateCooldown` subtracts one-way ping (half of `PlayerState` ping, clamped to 0.2s) so the bar matches what they pressed. Lobby sliders write `FPacketSimulationSettings` on the client net driver (`PktLagMin` / `PktIncomingLagMin`) without a rebuild.

```
MobaProject/
  Source/MobaProject/   C++ gameplay, session, HUD
  Content/Moba/         maps, hero BPs, abilities, art, SFX
  Config/
```

Lane AI is `AMobaMinionAIController`. The pawn keeps mesh, GAS, montage, and death. Aggro, leash, and focus-counting live on the controller so `Tick` on the character is not the brain. Towers pull aggro when a hero hits a hero; otherwise they shoot the closest enemy. The match ends when `AMobaVictoryManager` sees a team tower die.

## Reasoning

Blueprint was how I started. It is a bad place to keep predicted melee, a listen host, and a dedicated server in the same project. The C++ is there so a second client is a real machine, not a PIE trick I cannot explain.

Unreal still does what it is good at: possessed pawn, Enhanced Input, replication, widgets, cooking three targets. The session code decides whether this process is allowed to listen. Ground slams do not re-trace the camera on the server; the client confirms the point it aimed. Shop gold only moves if you are in range (or dead) and the server agrees.

Hats look like a joke until you are the SimulatedProxy. Construction Script KeepWorld fires while Head is still at the feet, so the hat stores “stay 173 units above the origin.” When the skeleton updates, it floats. The cosmetic component waits until Head is actually on the skull, then converts mesh-space into Head-space. That is the kind of bug I like: ugly, local, and you can walk someone through it.

## Scope

Two heroes, one test arena, debug-draw FX, no matchmaking. Art and audio are a mix of project pieces and third-party SFX; credits are in [CREDITS.md](CREDITS.md). Paragon animation dumps are not in git. Montages under `Content/Moba` are enough to run the slice.

Made with C++ and Unreal 5.8.
