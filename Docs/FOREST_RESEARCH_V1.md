# Anomaly Hunter: Forest Research Facility v1 (UE 5.8)

## Design
**Small, mostly indoor** research installation reached from a short **outdoor woodland entrance**. Current AI behaviour is unchanged. The existing TestArena is never replaced.

Included on this branch:
- `Source/MyProject5/MyFPSPawn.h`: default `BaseFOV = 100.f`.
- `Tools/build_forest_research_ue58.py`: generates a completely new `/Game/Maps/AH_ForestResearch_Small` level.
- `Assets/ForestAnomaly/SK_AH_ForestAnomaly_Preview.obj` and `.mtl`: static, stylised faceless anomaly **preview**.
- `Tools/build_forest_anomaly_blender.py`: generates a skinned 21-ish-bone skeletal model, Blender source, and seven simple animation clips.
- `Tools/import_forest_anomaly_ue58.py`: imports the static preview, then the optional locally generated FBXs.

## Your screenshots versus Git
Your local Unreal Content Browser showed `Characters`, `Generated`, and `Maps`. Git does not necessarily contain their `.uasset` or `.umap` contents. This branch can use the source code and scripts on GitHub, but local-only Unreal assets will still require a local Unreal import/build.

## Safe setup, in order

1. Save current work and close UE Editor. In Git Bash at `C:/UE5Projects/MyProject5`, run:
   `git status --short` (resolve any uncommitted changes first), then
   `git fetch origin` and
   `git switch --track origin/feature/forest-research-facility-v1`.
   If the branch already exists locally, use `git switch feature/forest-research-facility-v1` and `git pull --ff-only`.
2. Build in Visual Studio with UE 5.8 **Development Editor | Win64**, while the Unreal Editor is closed. (Live Coding blocks an external build.)
3. Open Unreal Editor and run
   **Tools > Execute Python Script > `Tools/import_forest_anomaly_ue58.py`**.
   This first imports the committed static preview OBJ. Missing FBX files are harmless and logged.
4. Run
   **Tools > Execute Python Script > `Tools/build_forest_research_ue58.py`**.
   This creates `/Game/Maps/AH_ForestResearch_Small` and places a **visual-only, non-colliding** preview anomaly in its server room if the OBJ imported successfully.
5. In the Content Browser go to `Content/Maps/AH_ForestResearch_Small` and double-click to open. Press Play and test the entrance/hall/rooms. **The default map is intentionally unchanged.**
6. Optional animation asset generation: open Blender 4.x/5.x, enter Scripting,
   use **Text > Open** on `Tools/build_forest_anomaly_blender.py` and press Run Script
   (or from a terminal: `blender -b -P Tools/build_forest_anomaly_blender.py`).
   Opening it from `Tools/` matters: that is how the script finds where to write its exports.
   **The Blender script deletes objects in its active scene**: start with a blank file or save your work first.
   `Tools/Exports/` is git-ignored, so the generated files never get committed by accident.
   Exports are created in `Tools/Exports/ForestAnomaly/`:
   `SK_AH_ForestAnomaly.fbx`,
   `AN_AH_FA_Idle.fbx`, `AN_AH_FA_Walk.fbx`, `AN_AH_FA_Run.fbx`,
   `AN_AH_FA_Turn.fbx`, `AN_AH_FA_Alert.fbx`,
   `AN_AH_FA_Investigate.fbx`, `AN_AH_FA_Reach.fbx`,
   and `AH_ForestAnomaly_Rigged.blend`.
7. Run `Tools/import_forest_anomaly_ue58.py` again to import the skeletal mesh and clips, choosing **the imported skeleton** for any FBX animation dialogs in UE. Preview each clip on that skeleton. The rigged character is not automatically assigned to your C++ `AEnemy`, since enemy behaviour was intentionally deferred.
8. When ready to integrate gameplay later, create a Blueprint child of `AEnemy` or `ASmartEnemy` and assign this skeletal mesh. Keep the parent C++ AI logic unchanged. Set its relative mesh position/scale for the existing capsule and retest collisions/visibility.

## Layout
- Woodland approach, lit path, fence, trees, shelter and entrance vestibule.
- North wing: reception/security, laboratory, server room.
- South wing: storage, medical observation, generator maintenance.
- Connected central corridor with 240-cm side-room openings and accessible entrance.
- Modular cabinets, desks, monitors, shelving, floor grates, cable trays, warning strips, ceiling lights, equipment racks, pipes, industrial columns, rocks and trees.
- All built with engine primitive meshes for **robust availability**, so these are detailed **blockout assets**, not finished photoreal assets. Replace with authored materials/meshes and trim decals in further art passes.

## QA before merging
- Project builds with Development Editor Win64, editor closed.
- Existing TestArena still loads and plays.
- A new map is created; no previous map or local modified scene is overwritten.
- 100° FOV applies to the pawn in Play (Blueprint/instance overrides may take precedence).
- PlayerStart loads outside the facility facing its entrance, level collision works.
- Corridor and all six room doorways are traversable.
- Monitor FPS and actor counts; current individual primitive trees are placeholders and should eventually be replaced with instanced foliage.
- The non-AI static preview never blocks passage.
- Blender generator runs without errors, its skeleton deforms, and UE previews all seven clips on the same skeleton.
  (Checked in Blender 5.2, Nov 2026: runs clean; read back from the exported FBXs, Turn/Idle/Alert/Investigate
  turn the head and chest about the vertical, Walk/Run swing arms and legs forwards/back with the feet kept level,
  Reach raises the arms forward. Not yet imported into Unreal.)
- Actual enemy AI use of the new model is **not yet implemented**.

## Caveats
These scripts were authored and committed remotely, but no Windows Unreal 5.8 or Blender session is available in this environment. **No runtime, compilation, import, mesh skinning or animation export success is claimed before local testing.** Generated `.umap`/`.uasset` files are local unless separately added and pushed to Git.
