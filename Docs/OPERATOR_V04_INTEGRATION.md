# Anomaly Hunter Operator FP v0.4 | Unreal Engine 5.8

This upgrade is **opt-in** and does not require replacing any current asset, level, or gameplay system. The code patch in the feature branch adds a camera-mounted arms option that is **off by default**.

## What changed

- Independent left/right IK controls and elbow pole targets in Blender.
- Five editable, baked skeletal animation sequences: Idle, Walk, Run, Crouch, Inspect.
- Wrist and finger secondary motion for each hand.
- Non-weapon, fictional two-hand containment scanner asset (GLB/OBJ).
- An editor-only Unreal Python importer that **skips** already-existing assets.
- Optional `AMyFPSPawn` support for a procedural Animation Blueprint without overriding the old single-clip animation path.

## 0. Check engine and back up

`MyProject5.uproject` uses Unreal Engine **5.8**. No engine-association changes are required.

The GitHub branch is `feature/operator-fp-rig-v04` and must **not** be merged until you verify it in a separate clone or working tree. The default branch is unchanged.

## 1. Make the five animation FBXs in Blender

1. Open the rigged `AnomalyHunter_ContainmentOperator_Rigged.blend` in Blender. Use the one made by this repo's fixed rig script (`Tools/OperatorArms/rig_and_export_fbx_fixed.py`, output in `Tools/OperatorArms/Exports/`) so the clips match the FBX in `Assets/OperatorArms/`.
2. Open and Run `Tools/ah_operator_blender_animate.py` from the add-on package.
3. Look inside the NEW folder `Exports/Operator_v04/` for:
    - `AH_Operator_FP_v04_IK_Controls.blend`
    - `AH_FP_Idle.fbx`
    - `AH_FP_Walk.fbx`
    - `AH_FP_Run.fbx`
    - `AH_FP_Crouch.fbx`
    - `AH_FP_Inspect.fbx`
4. Open the new .blend. In Pose Mode move `CTRL_AH_Hand_l` and `CTRL_AH_Hand_r` independently. Move the corresponding `CTRL_AH_Elbow_l/r` to adjust bending direction.
5. Inspect elbows, wrists and fingers. If the elbow bends backwards, adjust the IK constraint's Pole Angle in Blender and re-bake before importing.

**Tested (Nov 2026):** the script has been run in Blender (bpy 5.2) against the fixed rig. It reads which side each arm is on and the IK pole angle from the rest pose, so it works on both the fixed rig (`_l` at +X) and the pack's original layout. All five clips bake with the arms on their own sides, no forearm twist and fingers curling towards the palm. The clips have not been imported into Unreal yet.

## 2. Copy assets into your local project

Place the files, without changing existing project assets:

```
MyProject5/
  Assets/
    OperatorFP/
      Animations/
        AH_FP_Idle.fbx
        AH_FP_Walk.fbx
        AH_FP_Run.fbx
        AH_FP_Crouch.fbx
        AH_FP_Inspect.fbx
      Scanner/
        SM_AH_ContainmentScanner.glb
      Textures/
        [copy the v0.3 operator PNG textures here]
  Tools/
    ah_operator_import_ue58.py
```

The scanner is a separate, static **diagnostic instrument**. It isn't part of the arms skeleton. Align it as a non-weapon prop between the gloved hands only after the arms look right.

## 3. Import into Unreal

1. Build and open your **UE 5.8** project.
2. Confirm the operator skeletal mesh is at `/Game/Generated/Operator/SK_AH_ContainmentOperator_FP` (run `Tools/ue_setup_project.py` if it isn't there). If yours lives elsewhere, edit `MESH_ASSET_PATH` in `Tools/ah_operator_import_ue58.py`. Do not use `/Game/character`: in MyProject5 that is the old Mixamo arms, a different skeleton.
3. In the editor use **Tools → Execute Python Script...** and choose `Tools/ah_operator_import_ue58.py`.
4. It imports the five animation FBXs against your existing skeleton (if they exist), textures, four PBR materials and the scanner GLB under `/Game/Characters/Operator_FP/`.
5. Open `AH_FP_Walk` in the Animation Sequence editor and check that elbows **move relative to shoulders** and the hands aren't simply translated with the camera.
6. If UE 5.8 uses a different FBX importer in your setup, import each FBX manually with **Import Mesh OFF**, **Import Animations ON**, and your **existing operator Skeleton** selected.
7. Assign the new materials to the operator skeletal mesh's material slots by appearance. Avoid changing material slot order if it is already referenced elsewhere.

**The import script will not create an Animation Blueprint node graph automatically** because that requires editor validation in your exact UE 5.8 build. Use the next step to set it up deliberately.

## 4. Opt-in runtime control without replacing the old player

The feature branch adds two new optional properties on your existing C++ `AMyFPSPawn`:

- `bEnableIndependentArmRig` (default: **false**)
- `ArmsProceduralAnimClass` (optional Animation Blueprint class)

With `bEnableIndependentArmRig=true`, the arms attach to the camera rather than the other visual component. The camera still establishes overall viewpoint, but individual arm bones can be posed independently.

The update also exposes read-only animation signals for movement, sprinting, crouching, and look input.

**After compiling the feature branch**, create a Blueprint subclass `BP_AH_OperatorPawn` of `MyFPSPawn`. In its Class Defaults:

1. Under **Arms | Independent**, turn on `bEnableIndependentArmRig`.
2. Keep `ArmsMeshAsset` set to the operator mesh you already imported.
3. For a first smoke test, assign `ArmsIdleAnim = AH_FP_Idle` and `ArmsWalkAnim = AH_FP_Walk`; leave `ArmsProceduralAnimClass` empty. The existing logic will play those two clips with the arms camera-mounted.
4. Create a dedicated `BP_AH_OperatorGameMode` child of your existing game mode and set its Default Pawn Class to `BP_AH_OperatorPawn`.
5. In **World Settings**, set GameMode Override to `BP_AH_OperatorGameMode` for a duplicate test level. Do not modify the existing map's default setup until you confirm the arms look correct.
6. If the arms face backwards, adjust **ArmsRotation / ArmsOffset / ArmsScale** on your new Blueprint subclass, not the mesh's actual geometry or the engine version.

The old pawn/level behaviour is unchanged until you select the new Blueprint in a test level.

## 5. Real-time IK and Control Rig (UE 5.8)

For fully procedural **live hand targets** rather than just baked animations:

1. Right-click your operator Skeletal Mesh → **Create > Control Rig**, name it `CR_AH_OperatorFP`.
2. Give the rig separate `hand_l` / `hand_r` effector controls and elbow-direction controls.
3. Use **Full Body IK** in Control Rig for your v0.3 chain. This chain has upperarm → lowerarm → forearm_twist → wrist → hand, so it is **not** an unbroken two-segment chain; simple Two Bone IK with `hand_l/r` as the end bone will not necessarily match the v0.3 hierarchy.
4. Create an Animation Blueprint on the existing operator skeleton (`ABP_AH_OperatorFP`).
5. In its AnimGraph: blend Idle/Walk/Run/Crouch as needed → **Control Rig** node (`CR_AH_OperatorFP`) → Output Pose. Pass hand targets to separate Control Rig controls and use elbow controls to maintain natural bend direction.
6. In the Event Graph, read the `GetArmMovementAlpha`, `GetArmIsRunning`, `GetArmIsCrouching`, and `GetArmLookInput` functions exposed by the optional pawn patch. Apply small smoothing/lag so the hands don't exactly track every camera jitter.
7. Assign this ABP class to `ArmsProceduralAnimClass` on `BP_AH_OperatorPawn`. The optional pawn code now lets the animation blueprint evaluate continuously rather than overriding it with `PlayAnimation` each movement-state change.

Do not use `AMyFPSPawn`'s existing animations for the new skeleton unless Unreal confirms their Skeleton compatibility.

## 6. Non-weapon scanner alignment

- The GLB's local X axis runs between the two grips, -Y points towards the front of the reader, and Z is up.
- The original v0.3 operator palms are close to X ±0.125 metres in Blender model space. This scanner's handle centres are X ±0.135 metres.
- Use a **separate scene/mesh component** for the scanner and align in the character Preview or level, then set hand target positions to the left/right grips.
- Do not adjust the operator skeleton to make the scanner fit. Treat the scanner as a separate swappable prop.

## 7. QA before merging Git branch

- [ ] Original MyProject5 launches on UE 5.8
- [ ] New arm mesh displays in a duplicate test map
- [ ] Both independent hand controls operate without elbow flipping
- [ ] Wrist and finger motion remains attached to the hands
- [ ] All animation FBXs import against the same skeleton
- [ ] `AH_FP_Walk` plays visibly different arm poses across its timeline
- [ ] Default `bEnableIndependentArmRig=false` preserves original gameplay
- [ ] Opt-in Blueprint works when enabled
- [ ] Scanner renders and aligns between the palms
- [ ] No existing level, blueprint, or asset was overwritten

## Known limitations

- The animation FBXs are generated locally from Blender, not pre-baked in this repository.
- The programmatic rig weights and constraints need a visual QA pass in Blender.
- An Unreal Control Rig graph is specified and documented but must be authored/tested in Unreal Editor. The GitHub branch cannot contain a verified `.uasset` generated without the Unreal Editor.
- The scanner is a procedural hard-surface model rather than a hand-sculpted hero prop.
- No gameplay systems or weapons are added by this upgrade.

Official engine references:
- https://dev.epicgames.com/documentation/unreal-engine/rigging-with-control-rig-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-animation-blueprints-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/animation-blueprint-two-bone-ik-in-unreal-engine

## Existing native operator IK: preferred path

Current MyProject5 already provides `UOperatorArmsComponent` with independent elbow, forearm, wrist, and finger posing. Its expected mesh path is `/Game/Generated/Operator/SK_AH_ContainmentOperator_FP`; `Tools/ue_setup_project.py` imports the fixed rig there. (`/Game/character` is the old Mixamo arms the pawn falls back to, not the operator rig.) The included `ah_operator_activate_existing_ik_ue58.py` helper is only for an operator mesh you imported by hand somewhere else: set its `SOURCE` to that asset. If the target path already exists, no changes are made. Otherwise, it safely duplicates the source skeletal mesh. Restart Play and inspect `OperatorArms:` messages in Output Log for any calibration failure. The existing repo includes a corrected fixed FBX in `Assets/OperatorArms/` if the older, uncorrected rig has mismatched bones/axes.

The Blender IK and Animation Blueprint procedure in this guide is an **alternative optional animation-authoring workflow**. It is not needed for the existing `UPoseableMeshComponent`'s live IK and should not be connected over it.
