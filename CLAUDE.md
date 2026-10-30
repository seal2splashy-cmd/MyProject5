# MyProject5 - Bodycam FPS Horror Game
## Unreal Engine 5.8 | C++

## Project Overview
- Bodycam-style FPS horror/action game (OCR A Level CS coursework + ongoing project)
- All gameplay logic is in C++, no Blueprints for core mechanics
- Legacy input (axis/action mappings in Config/DefaultInput.ini + direct BindKey calls)

## Classes
- AMyFPSPawn - APawn with manual movement, stamina, bodycam camera FX, health/death, FP arms, weapon slots
- AMyGameMode - sets AMyFPSPawn + AMyHUD
- AGun - pistol: fire/reload/recoil, first-person offset, GetMuzzleAimPoint (crosshair + bullets share it)
- ABullet - manual gravity + quadratic drag, sphere-swept collision
- AEnemy - ACharacter, 4-state FSM, predictive chase with adaptive bias, ragdoll death
- ASmartEnemy - AEnemy + Q-learning (FQLearningAgent + FPlayerTracker), Saved/QLearning/qtable.json
- AMyHUD - dynamic crosshair, health/stamina bars, ammo, death screen
- AMeleeWeapon - physics melee: spring-damper hand, mouse-steered guard, velocity-based damage
- UMeleeCombatComponent - glue between pawn and melee weapon, holds the weapon list
- AMeleePipe / AMeleeCrowbar / AMeleeMachete / AMeleeFireAxe (MeleeWeaponTypes) - built from engine shapes

## Assets
- Assets/ holds generated WAVs/PNGs (Tools/generate_assets.py); Tools/ue_setup_project.py imports them
  to /Game/Generated, builds materials and /Game/Maps/TestArena. Code loads them by soft path and
  silently skips anything missing. See README_ASSETS.md.

## Controls
WASD move, Shift sprint, Ctrl crouch (slide while sprinting), Space jump, Q/E head tilt, Z/C lean,
1 gun, 2 melee (press 2 again to cycle pipe/crowbar/machete/axe). Gun: LMB fire, RMB aim, R reload. Melee: hold RMB + move mouse to steer, LMB thrust.

## Still to do
- Hand IK onto the melee weapon (arms currently hide with the gun)
- Fear wobble + one/two-hand grip modes for melee
- Surface-specific impact sounds (bReturnPhysicalMaterial is already on in the melee sweep)
- NavMesh pathfinding (enemies use direct-line MoveTowards)
- Line of sight on by default (bUseLineOfSight)

## History
Source was lost in Oct 2026 and rebuilt from chat history. MyFPSPawn/AGun/MyHUD/MyGameMode are
the last full versions from those chats; ABullet/AEnemy/ASmartEnemy/FQLearningAgent/FPlayerTracker
were reconstructed from the coursework excerpts (same design, names and tuning values).
