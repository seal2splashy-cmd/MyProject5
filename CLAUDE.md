# MyProject5 - Bodycam FPS Horror Game
## Unreal Engine 5.8 | C++

## Project Overview
- Bodycam-style FPS horror/action game (OCR A Level CS coursework + ongoing project)
- All gameplay logic is in C++, no Blueprints for core mechanics
- Legacy input (axis/action mappings in Config/DefaultInput.ini + direct BindKey calls)

## Classes
- AMyFPSPawn - APawn with manual movement (walk 3 / sprint 6.5 m/s + momentum, slopes <= 45 deg, 40 cm step-up,
  ground snap going down), stamina, breathing, fear
  (heartbeat, hand tremor, vignette - from hunting enemies, the Gaunt, darkness, wounds), fall damage,
  per-surface footsteps (noise for enemies), lean collision, GetVisibility01 (light + torch) for enemy eyes,
  bodycam camera FX, health/death, FP arms, weapon slots (Docs/REALISM.md)
- AMyGameMode - sets AMyFPSPawn + AMyHUD
- AGun - pistol: real mags + chamber, tactical/empty reloads, spread cone, stacking upward recoil, ADS pose,
  muzzle flash light, gunshot noise; first-person offset, GetMuzzleAimPoint (crosshair + bullets share it).
  Mesh, first one imported wins: SM_AH_Pistol (Meshy), SM_AH_ServicePistol (Tools/build_pistol_blender.py),
  the field sidearm prop, the old Fab pistol; BeginPlay sets grip/muzzle offsets to match
- ABullet - manual gravity + quadratic drag, sphere-swept collision, penetration (wood/sheet metal/glass/leaves),
  ricochets, hit zones, per-surface impact sounds + noise
- AEnemy - ACharacter, FSM Patrol/Chase/Attack/Search/Dead, vision cone + lighting + awareness meter, hearing
  (static AEnemy::ReportNoise), own footsteps, predictive chase with adaptive bias, ragdoll death. MoveTowards
  follows NavMesh paths (straight lines with no NavMesh) (Docs/ENEMY_AI_NAVIGATION.md, Docs/REALISM.md).
  Console: ah.DebugSenses 1, ah.DebugNoise 1
- ASmartEnemy - AEnemy + Q-learning (FQLearningAgent + FPlayerTracker), Saved/QLearning/qtable.json
- AGauntAnomaly - ASmartEnemy with the Gaunt's body: rigged SK_AH_Gaunt + clips if imported, else the static
  stand-in with procedural motion; reports bodycam interference when near (Docs/GAUNT_ANOMALY.md)
- AMyHUD - bodycam REC/timestamp overlay, spread-aware crosshair (fades on ADS), health/stamina bars, ammo + mags,
  death screen, mission objective (top left) + messages + extraction screen when a mission director is in the level
- AAHInteractable (AHInteractable.h) - base for F-to-use things; the pawn traces from the camera, HUD shows "[F] ..."
- AAHDoor - single/double hinged door from engine shapes; opens away from you, sprint = slam, enemies open it,
  noises, locked doors block the NavMesh (Docs/DOORS_PICKUPS.md)
- AAHPickup - magazine (pouch, max 6) / bandage (heal over time) / torch battery; Tools/place_doors_pickups_ue58.py
  places doors + pickups in the forest maps
- AFacilityMissionDirector - the A-07 mission: 7 ordered steps (card, power, security door, decon, observation log,
  containment -> breach, extraction), HUD text, light roles by tag (AH_PoweredLight/EmergencyLight/FlickerLight),
  lifting security slab, airlock door lock, breach alarm + Gaunt release, checkpoints, restart, light buzz +
  AH_MachineHum generator rumble (Docs/A07_PLAYABLE.md)
- AFacilityInteractionPoint - an AAHInteractable mission device built from engine shapes (lockbox, breaker,
  card reader, decon panel, terminal, beacon) with a red/amber-pulse/green status light; F or G
- UAHMissionProgress - GameInstance subsystem holding the mission checkpoint across the death reload
- AHRealism (RealismUtils) - surface type from a hit (names), per-surface penetration/footstep/impact data,
  generated sound lookup, realistic attenuation (distance low-pass + wall occlusion)
- AMeleeWeapon - physics melee: spring-damper hand, mouse-steered guard, velocity-based damage
- UMeleeCombatComponent - glue between pawn and melee weapon, holds the weapon list
- AMeleePipe / AMeleeCrowbar / AMeleeMachete / AMeleeFireAxe (MeleeWeaponTypes) - built from engine shapes,
  or an imported ModelMesh when there is one (the pipe has the Meshy model)
- UOperatorArmsComponent - PoseableMesh FP arms, two-bone IK + finger curl onto gun/melee grips (no anims).
  Uses SK_AH_OperatorArms_Meshy (Meshy gloved forearms on the same rig) when imported, else
  SK_AH_ContainmentOperator_FP (bPreferMeshyArms; Docs/OPERATOR_ARMS_MESHY.md)
- UBodycamLensComponent - camera post-process bodycam look (barrel distortion via M_BodycamLens, fringing,
  grain, vignette, flat colour, slow exposure) + SetInterference / ReportInterference (driven by AGauntAnomaly)
- UBodycamTorchComponent - chest spot light on the camera: lagged aim, shimmer, battery with low-battery stutter
  (Docs/BODYCAM_FX.md)

## Assets
- Assets/ holds generated WAVs/PNGs (Tools/generate_assets.py); Tools/ue_setup_project.py imports them
  to /Game/Generated, builds materials and /Game/Maps/TestArena. Code loads them by soft path and
  silently skips anything missing. See README_ASSETS.md. Levels need a NavMeshBoundsVolume for enemy
  pathfinding: both level builders add one; Tools/add_navmesh_ue58.py adds one to the open level.
- Assets/OperatorArms holds the rigged Anomaly Hunter operator arms FBX + PBR maps (Tools/OperatorArms/README.md).
  If they aren't imported, the pawn falls back to the old Mixamo ArmsMesh.
- Meshy models: raw downloads go in Assets/Meshy (git-ignored); Tools/prepare_meshy_weapons_blender.py turns
  pistol.glb/pipe.glb into Assets/Weapons/SM_AH_Pistol/SM_AH_Pipe (real size, Unreal frame, pivots, measured
  grip/muzzle numbers, 2K textures with DirectX normals); Tools/import_meshy_weapons_ue58.py imports them.
  AGun and AMeleePipe use them automatically when imported (AMeleeWeapon::ModelMesh).
- Meshy arms: Assets/Meshy/arms/ (FBX + PNGs) -> Tools/build_meshy_arms_blender.py fits them onto the operator
  rig -> Assets/OperatorArmsMeshy/SK_AH_OperatorArms_Meshy.fbx + T_AH_MeshyArms_*; Tools/import_meshy_arms_ue58.py.
- A-07 map: duplicate AH_ForestResearch_MeshyPass01 as AH_ForestResearch_A07_Expanded, run
  Tools/build_a07_expanded_ue58.py, Tools/setup_a07_playable_ue58.py (doors, pickups, tags, Gaunt, sounds, NavMesh),
  then Tools/dress_a07_ue58.py (Meshy props + invisible collision boxes, light fittings, generator/light hum tags,
  reverb Audio Volumes, asset inventory in Saved/AH_AssetInventory.txt).
- A-07 kit: Assets/MeshyFacility/Kit (Tools/prepare_a07_kit_blender.py from the user's Meshy ZIPs: vessel, decon pod,
  console, vault door, wall panel, mount ring, catwalk - real size, front +Y, pivot bottom-centre, kit_report.json has
  the catwalk deck height) -> Tools/import_a07_kit_ue58.py -> /Game/ForestResearch/A07Kit ->
  Tools/build_a07_kit_level_ue58.py (walkable catwalks + stairs, wall cladding, cells, consoles, vessel, decon pod).
- Rugged surfaces: Tools/generate_rugged_textures.py -> Assets/Textures/Rugged (tileable concrete wall / worn floor) ->
  Tools/retexture_a07_ue58.py (M_AH_WorldAligned world-projected master + MI_AH_RuggedWall/WornFloor/StainedCeiling,
  swaps the blockout materials on the open level, volumetric dust fog so the torch beam shows).
- Story: Tools/pose_dead_operators_blender.py (rigged Meshy Tactical Operator -> 3 baked death poses in
  Assets/DeadOperators) + Tools/generate_blood_decals.py (Assets/Textures/Blood) -> Tools/place_story_ue58.py
  (bodies + M_AH_BloodDecal decals; the access card objective moves onto the storage body).
- Content/ is not in git: Claude only sees assets pushed under Assets/ (and Saved/AH_AssetInventory.txt if sent).

## Controls
WASD move, Shift sprint, Ctrl crouch (slide while sprinting), Space jump, Q/E head tilt, Z/C lean,
1 gun, 2 melee (press 2 again to cycle pipe/crowbar/machete/axe). Gun: LMB fire, RMB aim, R reload. Melee: hold RMB + move mouse to steer, LMB thrust.
F use (doors, pickups, mission devices; G also works on mission devices), T torch.

## Still to do
- One/two-hand grip modes for melee
- Rolling-shutter lens effect; reverb volumes exist only in the A-07 map so far (Tools/dress_a07_ue58.py)

## History
Source was lost in Oct 2026 and rebuilt from chat history. MyFPSPawn/AGun/MyHUD/MyGameMode are
the last full versions from those chats; ABullet/AEnemy/ASmartEnemy/FQLearningAgent/FPlayerTracker
were reconstructed from the coursework excerpts (same design, names and tuning values).
