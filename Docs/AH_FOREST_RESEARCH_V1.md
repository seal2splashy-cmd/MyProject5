# Anomaly Hunter: Forest Research Facility v1 (UE 5.8)

A **small, highly furnished indoor research complex with one outdoor forest entrance**.
This branch intentionally includes the earlier sidearm/IK tuning branch as its base.
No existing level, enemy state machine, Q-learning code or default game map is overwritten.

## New assets and scripts

| File | Purpose |
| --- | --- |
| Tools/ah_build_forest_research_ue58.py | Creates /Game/Maps/AH_ForestResearch_Small with modular props and materials |
| Tools/ah_generate_observer_blender.py | Builds masked humanoid Observer rig and eight animation FBX clips in Blender |
| Tools/ah_import_observer_ue58.py | Imports rig and clips to /Game/AH_ForestResearch/Enemy, skipping assets already present |
| Source/MyProject5/MyFPSPawn.h | Sets the default BaseFOV to 100 degrees |
| Tools/import_operator_sidearm_ue58.py | Previous-branch fictional prop importer, unrelated to this map |

The Unreal map exists **after** running the builder in UE. GitHub currently holds
the script that creates it, not a pre-built .umap. Likewise, the humanoid rig
and clips are generated locally by Blender; no .fbx binaries are committed.

## Stage 0: Back up and build

From Git Bash (close the Unreal Editor first to avoid Live Coding's exit-code-6
build refusal):

    cd /c/UE5Projects/MyProject5
    git status --short
    git fetch origin
    git switch --track origin/feature/forest-research-facility-v1

If the branch already exists locally, use:
    git switch feature/forest-research-facility-v1

**Do not switch while git status lists uncommitted work** without saving it.
Build MyProject5Editor in Visual Studio using Development Editor | Win64.
Open the project in Unreal Engine 5.8.

## Stage 1: Create the map

Unreal Editor > Tools > Execute Python Script... >
C:\UE5Projects\MyProject5\Tools\ah_build_forest_research_ue58.py

The script:
- Creates only a NEW level if absent, and refuses to overwrite it.
- Builds an exterior path, forest, fence, entry checkpoint and canopy.
- Builds roof, floors, corridor and six accessible research/utility rooms.
- Adds sample cabinets, benches, panels, server racks, warning stripes, equipment,
  conduits, grates, doorway frames and small signs/indicators.
- Adds lighting, mist, sky and a bodycam-compatible post-process.
- Adds a PlayerStart outside the entrance.
- Does not create enemy AI actors or alter enemy behavior.

Open Content Browser > Maps > AH_ForestResearch_Small.
The old TestArena and default startup map settings remain unchanged.
Use Play dropdown > Current Level to test the new map.

This is a **detailed modular greybox**, not photorealistic final art:
it uses engine basic shape meshes and flat PBR-colored materials.
Iteratively replace props and materials only after collision and lighting QA.

## Stage 2: Generate the Observer model and animation clips

In Blender 4.0+, start with a NEW unsaved scene. The generator clears the
current scene, so back up any open Blender project first.

Open C:\UE5Projects\MyProject5\Tools\ah_generate_observer_blender.py
from the Scripting workspace and click Run Script.

Expected output: Assets/ForestResearch/Enemy/Exports/

- SK_AH_Observer.fbx (skinned mesh + skeleton)
- AN_AH_Observer_Idle.fbx
- AN_AH_Observer_Walk.fbx
- AN_AH_Observer_Run.fbx
- AN_AH_Observer_Turn.fbx
- AN_AH_Observer_Alert.fbx
- AN_AH_Observer_Investigate.fbx
- AN_AH_Observer_Reach.fbx
- AN_AH_Observer_Retreat.fbx
- AH_Observer_Rigged.blend

The design is an original, non-graphic masked humanoid for a horror game.
Animation exports are in-place prototype clips with articulated, rigid-weighted
segments, not motion-capture-grade final animation. Actual deformation
needs local Blender and Unreal QA.

## Stage 3: Import rig and animations in UE 5.8

Run Tools/ah_import_observer_ue58.py from the UE Editor Python menu.
Expected destination: /Game/AH_ForestResearch/Enemy

Inspect the Skeletal Mesh and Skeleton Tree. The figure should be about 2.35
metres tall, with spine, shoulder/arm, head, leg, and foot bones.

Check animation previews individually, especially idle/walk/run.
UE 5.8 may use the Interchange importer instead of legacy FBX APIs. If the
script doesn't create the correct assets, use Content Browser > Import for
SK_AH_Observer.fbx, enable Skeletal Mesh and choose NEW Skeleton, then import
each animation FBX against that exact Skeleton with Import Mesh disabled.

## Stage 4: Optional visual-only enemy test

This branch does NOT replace AEnemy or ASmartEnemy, and does not create AI
behavior or a runtime Animation Blueprint. Enemy behavior was deferred.

Drag the imported Skeletal Mesh into the map as a Skeletal Mesh Actor, set
Animation Mode to Use Animation Asset and choose Idle, and turn collision off.
Later, make an AEnemy Blueprint child and assign this rig and a compatible
Animation Blueprint before wiring locomotion, detection or other behavior.

## QA before merging

1. Build the project with the Unreal editor closed.
2. New map opens and old TestArena is unchanged.
3. Walk into all six rooms with no missing floors or blocked doorways.
4. PlayerStart leads to a traversable exterior entrance and interior corridor.
5. Dark lighting remains readable; adjust high-contrast areas.
6. Map-only materials are under /Game/AH_ForestResearch.
7. Skeleton, mesh and every animation preview work.
8. No missing imports, duplicate assets or overwritten files.
9. FOV reaches 100 degrees: any serialized Blueprint override may need
   its BaseFOV edited in Blueprint Class Defaults.
10. Monitor frame rate; reduce shadow-casting lights and foliage actors if needed.

After local testing, save the map and any new Unreal materials/animations.
To put the GENERATED files on GitHub too, stage the .umap and .uasset files
under Content/Maps/AH_ForestResearch_Small.umap and Content/AH_ForestResearch/,
commit and push them from this feature branch.

## Known limitations

The GitHub source and Python scripts exist, but Blender 4.x/5.x export,
Unreal 5.8 Editor execution and in-game QA have not been performed here.
The source is an actionable first-pass build, not a verified production-ready
level or final character animation.

Avoid running Tools/ue_setup_project.py for this level. That separate script
regenerates the TestArena and other existing generated resources.
