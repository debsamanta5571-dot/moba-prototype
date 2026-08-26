# Moba Prototype
I've always wanted to create a 3d multiplayer moba. I've been experimenting with blueprint on and off over the years. Here is a c++ prototype of a project

![Gameplay](gameplay.gif)

Unreal Engine **5.8** C++ MOBA slice: two networked heroes, GAS abilities, a shop, minion waves, and towers. Built as a gameplay / netcode portfolio piece, not a shipped game.

Listen-server host, a client-only build that cannot host, and a dedicated server are all supported.

## What it demonstrates

- **Gameplay Ability System** — predicted activate, montage notifies, cooldowns on the ASC (not the CDO)
- **Net modes** — standalone host, listen server, dedicated server, client-only target
- **Abilities** — trace (melee / flamestrike), projectiles, ground AoE, beam, dash (CMC root motion)
- **Combat** — server-authoritative damage, ping-adjusted cooldown bars, shop gold on the server
- **Match flow** — lobby, hero/team pick, wait-for-players, late join loadout, play again / return to menu

Ability VFX are `DrawDebug` shapes on purpose. That is prototype presentation, not Niagara.

## Engine

This `.uproject` is associated with a **source-built Unreal 5.8**, not the Epic Games Launcher install.

- Engine: Unreal 5.8 from source
- Open `MobaProject/MobaProject.uproject` in that editor
- Generate Visual Studio files from that engine if you build from the IDE

Launcher 5.8 will not find the module until you retarget the engine association.

Paragon animation assets are **not** in this repo (~3.8 GB). Montages in `Content/Moba` are enough to run the slice. If a mesh is missing an anim, copy a local Paragon Manny pack into `Content/ParagonAnims/` (gitignored).

## How to play (editor)

1. Open `Moba/Maps/MobaMenu`.
2. **Host** starts a listen lobby. Pick Brawler or Mage and a team, then **Start**.
3. A second PIE window or packaged client **Join**s `127.0.0.1:7777`.
4. First player on a dedicated server is the lobby leader.

### Packaged (if you have the three builds)

| Build | Exe | Role |
|---|---|---|
| Standalone | `MobaProject.exe` | Can host (listen) or play local |
| Client | `MobaProjectClient.exe` | Join only — Host is hidden |
| Dedicated | `MobaProjectServer.exe` | Headless. `StartServer.bat` listens on **7777** |

Client: Join `host-ip:7777`. Dedicated Play Again does **not** add `?listen`.

## Controls

| Input | Action |
|---|---|
| WASD | Move |
| Mouse | Look |
| LMB / ability slots (Q, E, …) | Cast (hold to aim ground abilities) |
| Shop key | Open shop (in fountain, or while dead) |
| Inventory / Desc / Settings | Overlay panels |

Exact key labels are on the ability HUD.

## Layout

```
MobaProject/
  Source/MobaProject/   C++ (GAS, session, heroes, minions)
  Content/Moba/         maps, hero BPs, abilities, HUD art, SFX
  Config/
```

Useful starting files:

- `MobaGameplayAbility` — shared predicted cast path
- `MobaSessionSubsystem` — host / join / dedicated travel
- `MobaFrontEndSubsystem` — menu, lobby, loading
- `GA_MobaTrace` / `Projectile` / `GroundAoE` / `Beam` / `Dash`
- `AMobaMinionAIController` — lane aggro / leash

## Scope

Two heroes, one test arena, prototype debug-draw FX, no matchmaking. Third-party audio and Epic mannequin / Paragon credit is in [CREDITS.md](CREDITS.md).
