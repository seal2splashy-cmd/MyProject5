# Operator FP v0.4 (UE 5.8) | independent arms integration

This is a **non-destructive proposal** for the first-person containment operator.
It is isolated from existing gameplay, with the original `master` branch unchanged.

## Scope
- Use the imported v0.3 skeletal arms mesh and its 43-bone skeleton.
- Provide separate left/right hand IK controls and elbow pole targets.
- Bake independent arm, wrist, and finger motion into five skeletal clips:
  `AH_FP_Idle`, `AH_FP_Walk`, `AH_FP_Run`, `AH_FP_Crouch`, and `AH_FP_Inspect`.
- Import a separate original **non-weapon containment scanner** with two side grips.
- Add asset imports only inside `/Game/Characters/Operator_FP/`; preserve existing assets, maps, C++ and current pawn defaults.

## Asset creation (Blender)
The v0.4 add-on distributed in the ChatGPT conversation includes:
- `Tools/ah_operator_blender_animate.py`
- `Tools/ah_operator_import_ue58.py`
- `Tools/generate_containment_scanner.py`
- `Assets/Scanner/SM_AH_ContainmentScanner.glb` and a fallback OBJ

Open your v0.3 `AnomalyHunter_ContainmentOperator_Rigged.blend` in Blender,
then run the v0.4 animation generator through the Scripting workspace.
It creates `Operator_v04/AH_Operator_FP_v04_IK_Controls.blend` and five
`AH_FP_*.fbx` animation exports.

The operator skeleton has a three-section forearm chain, so choose
**Full Body IK** rather than assuming the hand can be controlled with
a simple 2-segment IK on its existing hierarchy.

## Safe import
Copy the generated animation FBXs under
`MyProject5/Assets/OperatorFP/Animations/`, the scanner GLB under
`MyProject5/Assets/OperatorFP/Scanner/`, and optionally the previous
operator texture PNGs under `MyProject5/Assets/OperatorFP/Textures/`.

Run the included `Tools/ah_operator_import_ue58.py` in the **UE 5.8
Editor** with `Tools > Execute Python Script`. It imports into the
new Operator_FP content directory and skips existing assets.

For live hand targets, create `CR_AH_OperatorFP` using the existing
Skeletal Mesh's **Create > Control Rig** action and an Animation
Blueprint `ABP_AH_OperatorFP`. Configure a Full Body IK pass with
separate left/right hand effectors and elbow controls. Use the imported
motion clips as the base pose before the procedural IK pass.

## Important integration limitation
The existing `AMyFPSPawn` plays idle/walk assets through
`ArmsMesh->PlayAnimation`. It does not yet automatically evaluate a
newly-created Animation Blueprint in all states. Do **not** replace
that pawn's existing animation setup without a dedicated local test.
Preview the new clips on a duplicate level first, and make any
optional player hookup only after checking compatibility.

## QA gate
- Both hands can be moved independently in Blender.
- Pole controls guide elbow bend without flipping.
- Fingers remain attached under curl.
- Skeletal animation FBXs use the same skeleton as the imported v0.3 mesh.
- The scanner model displays separately.
- Existing project launches and gameplay is unchanged.
- Manual Control Rig asset and runtime hookup are validated **in UE 5.8**.

**Tooling constraints:** The add-on scripts were syntax-checked and
the static GLB exported and parsed. Blender and UE Editor are not
installed in the build environment, so animations and `.uasset`
Control Rig graphs still require local execution and QA. No
production-ready animation result is claimed before those tests.
