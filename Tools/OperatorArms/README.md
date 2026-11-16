# Operator arms (Anomaly Hunter Containment Operator)

First-person arms from `AnomalyHunter_Operator_AssetPack_v0_3_0.zip`, rigged and driven by IK
from C++ (`UOperatorArmsComponent`). No animations, no Animation Blueprint.

## What's in the repo

| Path | What |
|---|---|
| `Assets/OperatorArms/SK_AH_ContainmentOperator_FP.fbx` | Rigged skeletal mesh, 43 bones, 76k verts, textures not embedded |
| `Assets/OperatorArms/Textures/` | The pack's 2K/1K PBR maps (BC, N, R, AO, Height) |
| `Tools/OperatorArms/rig_and_export_fbx_fixed.py` | Blender script that builds the FBX from the pack (fixed version) |
| `Tools/OperatorArms/preview_ik_pose.py` | Blender prototype of the C++ IK, renders test poses |
| `Tools/OperatorArms/preview_*.png` | Those test renders (pipe grip, pistol grip, hand close-ups) |

`Tools/ue_setup_project.py` imports the FBX + textures, builds six materials and assigns them.

## Fixes made to the pack

The pack's own Blender script had never been run. Running it as supplied would have produced
a broken rig, so `rig_and_export_fbx_fixed.py` changes:

1. **Axis.** The GLB stores a Z-up/-Y-forward model inside a Y-up glTF, so Blender imports the
   arms lying along -Z while the script builds the bones along -Y. Mesh and bones never met.
   The mesh is now rotated back into the authoring frame first (checked: shoulder caps land
   exactly on the shoulder joints).
2. **Thumbs.** The thumb, thumb pad/web/saddle/stitches and the thenar mound were beside the
   *little* finger, with the hypothenar mound beside the index - anatomically backwards for a
   palm-down hand. Those parts are mirrored across each hand's centre line and the thumb bones moved.
3. **Sides.** With -Y forward and Z up, +X is the operator's left, but the pack named it `_R`.
   Bones are named by the real side, so `hand_r` is the right hand.
4. **Textures** are no longer embedded (the FBX went from ~80 MB to 9 MB), and the two unused
   material slots are removed.

The normal maps were checked against the height maps: they're DirectX (green-down) already,
which is what Unreal wants, so no flip is needed.

## How the arms are driven

Every frame `UOperatorArmsComponent`:
1. asks the gun (`AGun::GetHandTargets`) or melee weapon (`AMeleeWeapon::GetHandGrip`) where each hand goes
2. orients the hand so index->pinky runs down the handle and the knuckles follow the arm
3. places the wrist so the fist centre lands on the grip, then solves two-bone IK with an elbow pole
4. curls the fingers (and opposes the thumb) around the grip, sharing hand roll with the forearm twist bone

Because the melee grip *is* the physics-simulated hand position, the arms swing with the weapon.
Switching weapons blends each hand to its new grip over 0.2 s.

## Looseness

The hands always land exactly on the weapon; everything behind them moves with inertia:

- the body (shoulders/torso) is a spring that trails the camera when you turn or move
- walk bob and breathing
- the torso turns towards the hands, so a wind-up uses the whole body
- the body leans in past `ComfortReach`, keeping elbows bent instead of locked straight
- each shoulder follows its hand a little (clavicle), the elbow drags behind fast hand movement
- fingers relax when still, tighten when swinging; trigger finger squeezes on each shot; shots jolt the shoulders

`Looseness` scales all of it: 0 = welded to the camera, 1 = default, 2 = floppy.
`BodySpringHz` (lower = heavier) and `BodySpringDamping` (lower = more wobble) set the character.

## Tuning (select the pawn in PIE, OperatorArms component)

- `ArmsScale`, `ShoulderOffset` - size and where the shoulders sit relative to the camera
- `ElbowPole` - which way the elbows point
- `JointCurlDegrees`, `GripAlong`, `GripPalmDepth` - fist shape / where the handle sits in the hand
- `TriggerFingerCurl`, `RelaxedCurl`, `IdleHandOffset`
- `bDrawDebug` - green = grip, orange = elbow, cyan = handle axis

On the gun: `GripOffset`, `GripAxis`, `SupportHandOffset` (Gun|Hands) - set these to match your pistol mesh.
On each melee weapon: `bTwoHanded`, `MainHandOffset`, `SecondHandOffset` (Melee|Hands).

## Regenerating the FBX

Unzip the pack into `Tools/OperatorArms/AnomalyHunter_Operator_AssetPack/`, then either
`blender -b -P Tools/OperatorArms/rig_and_export_fbx_fixed.py` or, with `pip install bpy`,
`python Tools/OperatorArms/rig_and_export_fbx_fixed.py`. Output goes to `Tools/OperatorArms/Exports/`;
copy the FBX into `Assets/OperatorArms/` and re-run `ue_setup_project.py`.
