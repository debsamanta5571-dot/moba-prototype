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

## What you are looking at

This is the kind of work I want to keep doing: prototype a feel, put it on the network, then clean up the bugs that only show up with two machines.

**Abilities.** One predicted GAS parent (`UMobaGameplayAbility`) runs the cast: energy, montage, notify, plant-in-place, cooldown on the Ability System Component. Children are types, not hero names — trace (slash / flamestrike), projectile, ground AoE, beam, dash. Dash goes through Character Movement so the SavedMove includes the root motion. Hits write health only on the server. Local bolts are cosmetic.

**Combat loop.** Gold on last hit, kill credit window, shop offers that change stats, ping-shaved cooldown bars so a laggy client’s HUD matches what they felt. Minions aggro, leash, and attack from an AI controller, not from `Tick` on the pawn. Towers chew a percent of minion max HP so the wave still dies as they level.

**Session.** Same binary can be a listen host, a client that is not allowed to host, or a dedicated server. Lobby caches hero and team across `ServerTravel`. The arena waits until the lobby’s player count has loaded (or 20 seconds, so a DC in travel does not hang forever).

## Architecture

Unreal draws the match. Gameplay lives in C++.

- `UMobaGameplayAbility` — shared predicted cast path. Cooldown is an ASC tag, not a timer on the ability object (`CanActivate` can run on the CDO).
- `GA_MobaTrace` / `GA_MobaProjectile` / `GA_MobaGroundAoE` / `GA_MobaBeam` / `GA_MobaDash` — the kits.
- `UMobaSessionSubsystem` — host, join, dedicated travel, lobby cache, simulated ping.
- `UMobaFrontEndSubsystem` — menu, lobby, loading movie, settings.
- `UMobaShopComponent` / `UMobaBeamComponent` / `UMobaCosmeticComponent` — economy, live beam, hat attach. The hero pawn stays the GAS avatar.
- `AMobaMinionAIController` — lane brain. The minion still owns mesh, health, and the attack montage.

Hero Blueprints (`BP_Brawler`, `BP_Mage`) fill slots, montages, and numbers. The rules of a swing, a bolt, or a buy are not Blueprint graphs.

```
MobaProject/
  Source/MobaProject/   C++ gameplay, session, HUD
  Content/Moba/         maps, hero BPs, abilities, art, SFX
  Config/
```

## Reasoning

Blueprint was how I started. It is a bad place to keep predicted melee, a listen host, and a dedicated server in the same project. The C++ is there so a second client is a real machine, not a PIE trick I cannot explain.

Unreal still does what it is good at: possessed pawn, Enhanced Input, replication, widgets, cooking three targets. The session code decides whether this process is allowed to listen. Ground slams do not re-trace the camera on the server; the client confirms the point it aimed. Shop gold only moves if you are in range (or dead) and the server agrees.

Hats look like a joke until you are the SimulatedProxy. Construction Script KeepWorld fires while Head is still at the feet, so the hat stores “stay 173 units above the origin.” When the skeleton updates, it floats. The cosmetic component waits until Head is actually on the skull, then converts mesh-space into Head-space. That is the kind of bug I like: ugly, local, and you can walk someone through it.

## Scope

Two heroes, one test arena, debug-draw FX, no matchmaking. Art and audio are a mix of project pieces and third-party SFX; credits are in [CREDITS.md](CREDITS.md). Paragon animation dumps are not in git. Montages under `Content/Moba` are enough to run the slice.

Made with C++ and Unreal 5.8.
