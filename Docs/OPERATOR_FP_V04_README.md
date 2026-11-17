# Anomaly Hunter Operator FP v0.4 (Unreal Engine 5.8)

**Non-destructive arm animation and containment scanner add-on.**

The existing v0.3 operator rig is used as the base. This add-on includes two-hand Blender IK control generation, an animation baking/export script, a procedural non-weapon scanner mesh (`.glb` and `.obj`), an Unreal 5.8 editor asset importer, and a thorough integration checklist.

The Git branch `feature/operator-fp-rig-v04` also contains **opt-in**, disabled-by-default animation hooks for `MyFPSPawn`; no default gameplay behavior is changed.

## Quick start

1. In Blender, open your existing `AnomalyHunter_ContainmentOperator_Rigged.blend` from v0.3.
2. Run `Tools/ah_operator_blender_animate.py` in Blender Scripting. The output is `Operator_v04/` next to the open .blend.
3. Inspect both arm IK effectors (`CTRL_AH_Hand_l/r`) and elbow targets (`CTRL_AH_Elbow_l/r`).
4. Copy `AH_FP_*.fbx` into `MyProject5/Assets/OperatorFP/Animations/`.
5. Copy `Assets/Scanner/SM_AH_ContainmentScanner.glb` into `MyProject5/Assets/OperatorFP/Scanner/`.
6. Copy the original v0.3 operator PNG textures into `MyProject5/Assets/OperatorFP/Textures/` if you want the importer to create new PBR materials.
7. Copy `Tools/ah_operator_import_ue58.py` to `MyProject5/Tools/`, launch your UE 5.8 project and execute it via **Tools → Execute Python Script**.
8. Inspect `AH_FP_Walk`, `AH_FP_Idle`, the material slots and the scanner prop. Do **not** overwrite your existing operator skeletal mesh.
9. If testing the feature branch, make a **Blueprint subclass** of `MyFPSPawn`. Set `bEnableIndependentArmRig=true` only in that subclass. Select it only in a duplicate test level. Follow the guide in `Docs/OPERATOR_V04_INTEGRATION.md`.

## Scope / accuracy

- The scanner GLB and OBJ are **already generated** and can be imported immediately.
- Python scripts are syntax-checked. Blender and Unreal Editor are not installed in this environment; their results **must be tested locally**.
- The animation FBXs and Control Rig `.uasset` **are not included as already generated files**. The Blender generator produces the FBX clips; the Unreal Control Rig graph is configured in Editor following the provided directions.
- This does not add weapon assets, weapon code, or gameplay mechanics.
- The `master` branch is untouched. The separate feature branch is for review.

## Files

- `Tools/ah_operator_blender_animate.py`: Blender IK controls, hand/finger animation, baked FBX exporter
- `Tools/ah_operator_import_ue58.py`: UE editor importer that skips existing assets
- `Tools/generate_containment_scanner.py`: source generator for the included static GLB and OBJ
- `Assets/Scanner/SM_AH_ContainmentScanner.glb`: editable non-weapon hard-surface model
- `Assets/Scanner/SM_AH_ContainmentScanner.obj`: fallback model
- `Docs/OPERATOR_V04_INTEGRATION.md`: manual editor setup, QA and optional pawn integration

## Your project already has a native IK component

The current MyProject5 source includes `UOperatorArmsComponent` and a fixed rigged FBX under `Assets/OperatorArms`. Its arm IK, wrist, and finger posing do not require an Animation Blueprint. **Activate that component first** by running `Tools/ue_setup_project.py` (Unreal Editor > Tools > Execute Python Script), which imports the fixed rig to the native component's expected `/Game/Generated/Operator/SK_AH_ContainmentOperator_FP` path. `Tools/ah_operator_activate_existing_ik_ue58.py` is only for an operator mesh you imported by hand elsewhere: set its `SOURCE`, and it copies that mesh to the expected path only if the destination is missing. It refuses `/Game/character`, which is the old Mixamo arms. It preserves the source and existing assets. Restart Play, then inspect `OperatorArms:` entries in the Output Log. If calibration fails, prefer the repository's fixed rigged operator FBX rather than the original v0.3 export.

The v0.4 Blender rig generator and animation clips are optional authoring extras and should **not** simultaneously override the native `UPoseableMeshComponent` pose. Blender is needed to evaluate and test those clips.
