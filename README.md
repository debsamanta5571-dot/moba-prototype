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

Hero Blueprints (`BP_Brawler`, `BP_Mage`) fill slots, montages, and numbers. The swing, the bolt, and the buy are C++.

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

### Combat data

`UMobaAttributeSet` replicates health, energy, gold, regen, damage modifier, CDR, resist, and move speed. Status (stun / slow / haste) is a component with a multiplicative slow stack. Slow magnitudes over 1 are treated as percent (20 → 20%).

Shop is not HUD. `UMobaShopComponent` owns range, catalog, and `ServerBuyOffer`. Repeat buys cost `base * 1.2^n`. You can buy in fountain overlap, or while dead. Repeat CDR/resist caps at 0.8 / 0.9.

Towers hitting minions deal a percent of **max** HP so wave clear stays even as minions level. Last-hit gold uses a kill-credit window on whoever damaged the victim.

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

Lane AI is `AMobaMinionAIController`. The pawn keeps mesh, GAS, montage, and death. Aggro, leash, and focus-counting live on the controller so `Tick` on the character is not the brain.

## Reasoning

Blueprint was how I started. It is a bad place to keep predicted melee, a listen host, and a dedicated server in the same project. The C++ is there so a second client is a real machine, not a PIE trick I cannot explain.

Unreal still does what it is good at: possessed pawn, Enhanced Input, replication, widgets, cooking three targets. The session code decides whether this process is allowed to listen. Ground slams do not re-trace the camera on the server; the client confirms the point it aimed. Shop gold only moves if you are in range (or dead) and the server agrees.

Hats look like a joke until you are the SimulatedProxy. Construction Script KeepWorld fires while Head is still at the feet, so the hat stores “stay 173 units above the origin.” When the skeleton updates, it floats. The cosmetic component waits until Head is actually on the skull, then converts mesh-space into Head-space. That is the kind of bug I like: ugly, local, and you can walk someone through it.

## Scope

Two heroes, one test arena, debug-draw FX, no matchmaking. Art and audio are a mix of project pieces and third-party SFX; credits are in [CREDITS.md](CREDITS.md). Paragon animation dumps are not in git. Montages under `Content/Moba` are enough to run the slice.

Made with C++ and Unreal 5.8.
