# Operator FP: smoother camera motion + fictional sidearm prop (UE 5.8)

This branch is a **reviewable experiment**, not an automatic upgrade to the
current playable game. No original assets, maps or project configuration are
replaced by the import helper.

## What was actually changed

1. New game-only sci-fi prop in `Assets/OperatorSidearm/`:
   - `SM_AH_FieldSidearm.obj`: closed-shell stylized design with dark casing,
     glowing stripes, wide main handhold and offset support handhold
   - `SM_AH_FieldSidearm.mtl`: seven material color groups
2. `Tools/import_operator_sidearm_ue58.py`: a **skip-existing**, editor-only
   import into `/Game/Generated/Operator/Props/SM_AH_FieldSidearm`.
3. `Source/MyProject5/AGun.cpp`: checks for that optional asset at BeginPlay.
   If found, displays it with **no change to existing gameplay**, using the
   new prop's +X forward orientation and its authored main/support grip positions.
   Otherwise leaves the previous imported visual and offsets intact.
4. `Source/MyProject5/OperatorArmsComponent.h`: toned down oscillation, lag,
   body rotation and bob. The independent elbow/finger IK solver is unchanged.

## How to test locally

1. Save a backup or commit local changes, then fetch and switch to
   `feature/smoother-operator-and-fictional-sidearm`. Rebuild the UE 5.8
   editor target.
2. Open `MyProject5.uproject` with UE 5.8.
3. In Unreal choose **Tools > Execute Python Script...** and select
   `Tools/import_operator_sidearm_ue58.py`. This only imports a new prop.
4. Verify `/Game/Generated/Operator/Props/SM_AH_FieldSidearm` exists. Check
   the preview is pointed along +X, +Z is up and the part is correctly sized.
5. Stop and restart **Play in Editor**, so `AGun::BeginPlay()` can find the
   static mesh. The left-hand support grip should be visibly offset from the
   right-hand grip.
6. In PIE, inspect the player's **OperatorArms** component. New C++ defaults
   apply only when no instance/Blueprint has overridden the values. If a
   Blueprint already overrides those fields, set `BodySpringDamping=1.05`,
   `Looseness=0.75`, `TurnLagSeconds=0.025` and
   `ElbowDrag=0.0015` to compare behaviour.

### Troubleshooting

- **Original prop still appears:** check the Output Log for
  `AGun: optional operator sidearm visual loaded.` and verify the exact
  imported asset name/path. Rebuild the module after switching branch.
- **Arms remain rigid:** ensure the project has the skeletal mesh at
  `/Game/Generated/Operator/SK_AH_ContainmentOperator_FP`, with
  the expected bone names; the legacy fallback arms are attached to the old
  prop and will remain rigid if the operator IK mesh fails to initialise.
- **Hands still intersect:** enable `bDrawDebug` on the OperatorArms
  component and check green target markers against both model handholds.
- **Wrong scale/orientation:** the mesh is authored in centimetres,
  +X forward and +Z up. Disable any importer axis conversion that changes
  its orientation.
- **Color missing:** Unreal's OBJ/MTL import may not reconstruct all material
  slots. Assign the seven named material colors manually in a test material
  instance, or keep the greybox look until the geometry and IK are right.
- **Strange bobbing despite branch changes:** Blueprint/instance serialized
  property values may override the new C++ defaults.

## Test limitations

Checked: versioned source changes, OBJ/MTL source geometry, imported file
path relationships, and the intended fallback logic. **Not yet checked:**
UE 5.8 compilation, asset import success, hand deformation in motion, color
assignment, build packaging, or runtime visuals. Do not merge until
local testing is complete.

The prop is a **nonfunctional fictional science-fiction asset**. No
mechanism, functional internals, or hardware assembly details are provided.
