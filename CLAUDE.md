# MyProject5 - Bodycam FPS Horror Game
## Unreal Engine 5.8 | C++

## Project Overview
- Bodycam-style FPS horror/action game (OCR A Level CS coursework + ongoing project)
- All gameplay logic is in C++, no Blueprints for core mechanics
- Legacy input (axis/action mappings in Config/DefaultInput.ini + direct BindKey calls)

## Classes
- AMyFPSPawn - APawn with manual movement, stamina, bodycam camera FX, health/death, FP arms, weapon slots
- AMyGameMode - sets AMyFPSPawn + AMyHUD
- AGun - pistol: fire/reload/recoil, first-person offset, GetMuzzleAimPoint (crosshair + bullets share it).
  Mesh, first one imported wins: SM_AH_Pistol (Meshy), SM_AH_ServicePistol (Tools/build_pistol_blender.py),
  the field sidearm prop, the old Fab pistol; BeginPlay sets grip/muzzle offsets to match
- ABullet - manual gravity + quadratic drag, sphere-swept collision
- AEnemy - ACharacter, 4-state FSM, predictive chase with adaptive bias, ragdoll death
- ASmartEnemy - AEnemy + Q-learning (FQLearningAgent + FPlayerTracker), Saved/QLearning/qtable.json
- AGauntAnomaly - ASmartEnemy with the Gaunt's body: rigged SK_AH_Gaunt + clips if imported, else the static
  stand-in with procedural motion; reports bodycam interference when near (Docs/GAUNT_ANOMALY.md)
- AMyHUD - dynamic crosshair, health/stamina bars, ammo, death screen
- AMeleeWeapon - physics melee: spring-damper hand, mouse-steered guard, velocity-based damage
- UMeleeCombatComponent - glue between pawn and melee weapon, holds the weapon list
- AMeleePipe / AMeleeCrowbar / AMeleeMachete / AMeleeFireAxe (MeleeWeaponTypes) - built from engine shapes,
  or an imported ModelMesh when there is one (the pipe has the Meshy model)
- UOperatorArmsComponent - PoseableMesh FP arms, two-bone IK + finger curl onto gun/melee grips (no anims)
- UBodycamLensComponent - camera post-process bodycam look (barrel distortion via M_BodycamLens, fringing,
  grain, vignette, flat colour, slow exposure) + SetInterference / ReportInterference (driven by AGauntAnomaly)
- UBodycamTorchComponent - chest spot light on the camera: lagged aim, shimmer, battery with low-battery stutter
  (Docs/BODYCAM_FX.md)

## Assets
- Assets/ holds generated WAVs/PNGs (Tools/generate_assets.py); Tools/ue_setup_project.py imports them
  to /Game/Generated, builds materials and /Game/Maps/TestArena. Code loads them by soft path and
  silently skips anything missing. See README_ASSETS.md.
- Assets/OperatorArms holds the rigged Anomaly Hunter operator arms FBX + PBR maps (Tools/OperatorArms/README.md).
  If they aren't imported, the pawn falls back to the old Mixamo ArmsMesh.
- Meshy models: raw downloads go in Assets/Meshy (git-ignored); Tools/prepare_meshy_weapons_blender.py turns
  pistol.glb/pipe.glb into Assets/Weapons/SM_AH_Pistol/SM_AH_Pipe (real size, Unreal frame, pivots, measured
  grip/muzzle numbers, 2K textures with DirectX normals); Tools/import_meshy_weapons_ue58.py imports them.
  AGun and AMeleePipe use them automatically when imported (AMeleeWeapon::ModelMesh).

## Controls
WASD move, Shift sprint, Ctrl crouch (slide while sprinting), Space jump, Q/E head tilt, Z/C lean,
1 gun, 2 melee (press 2 again to cycle pipe/crowbar/machete/axe). Gun: LMB fire, RMB aim, R reload. Melee: hold RMB + move mouse to steer, LMB thrust.
F torch.

## Still to do
- Fear wobble + one/two-hand grip modes for melee
- Surface-specific impact sounds (bReturnPhysicalMaterial is already on in the melee sweep)
- NavMesh pathfinding (enemies use direct-line MoveTowards)
- Line of sight on by default (bUseLineOfSight)

## History
Source was lost in Oct 2026 and rebuilt from chat history. MyFPSPawn/AGun/MyHUD/MyGameMode are
the last full versions from those chats; ABullet/AEnemy/ASmartEnemy/FQLearningAgent/FPlayerTracker
were reconstructed from the coursework excerpts (same design, names and tuning values).
