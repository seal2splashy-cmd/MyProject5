# The Gaunt (forest anomaly)

The tall, emaciated creature from `Anomaly_Hunter_Remesh` (Meshy), as a live enemy:
`AGauntAnomaly` = `ASmartEnemy`'s brain (FSM + Q-learning) + this body.

| | |
|---|---|
| Mesh | `Assets/Anomaly/Gaunt/SM_AH_Gaunt.fbx` - 10k triangles, 1.90 m, faces +Y like Mixamo/Blender characters |
| Textures | `T_AH_Gaunt_D/N/R.png`, 2K, cleaned up from Meshy's 4K: the "newspaper dot" pattern baked into the skin is filtered out, gaps in the atlas filled so seams don't go dark; the metallic map was empty and is dropped |
| Collision | capsule 34 x 95 (1.90 m) |
| Tuning | `MoveSpeed` 400, `AttackRange` 175, `EyeHeight` 80; everything else from `ASmartEnemy` |

## Getting it in the game

1. Build the C++ (editor closed).
2. Editor: Tools > Execute Python Script > `Tools/import_gaunt_ue58.py`
   (makes `/Game/Generated/Anomaly/Gaunt`: textures, `M_AH_Gaunt`, `SM_AH_Gaunt`).
3. Content Browser > C++ Classes > MyProject5 > drag **GauntAnomaly** into a level.
   A fresh run of `Tools/build_forest_research_ue58.py` places one in the server area by itself.

Until it's rigged it uses the **stand-in**: the static model, made to move procedurally - it
leans into its stride, bobs and sways as it walks, breathes when still, twitches at random
(twice as often while hunting you), lunges when it attacks, and topples over from the feet
when it dies. Close to you, it breaks up your bodycam picture (`Gaunt|Bodycam` settings).

## Rigging it with Mixamo (free)

1. Go to mixamo.com, sign in, **Upload Character**, pick `Assets/Anomaly/Gaunt/SM_AH_Gaunt.fbx`
   (it'll look grey there - textures don't matter, Unreal has them).
2. Place the markers (chin, wrists, elbows, knees, groin). Skeleton LOD: Standard.
   Its hands hang close to its thighs: in the preview, check the thighs don't stretch when the
   arms move; if they do, redo the markers with the wrist markers a little further out.
3. **Download** the character: FBX Binary, **With Skin**, T-pose. Save it as
   `Assets/Anomaly/Gaunt/Mixamo/Gaunt.fbx`.
4. Pick animations. Tick **In Place** on anything that walks or runs. Download each as FBX
   Binary, **Without Skin**, 30 fps, into the same `Mixamo` folder. Mixamo's names are
   understood (Zombie Walk, Mutant Run, Zombie Dying...). Good matches for this creature:
   - Idle: *Zombie Idle*, *Breathing Idle*
   - Walk: *Zombie Walk*, *Mutant Walking*
   - Run: *Mutant Run*, *Zombie Running*
   - Attack: *Zombie Attack*, *Mutant Swiping*
   - Death: *Zombie Dying*, *Dying*
5. Blender: run `Tools/rig_gaunt_from_mixamo_blender.py` (Text > Open, Run Script, or
   `blender -b -P Tools/rig_gaunt_from_mixamo_blender.py`). It puts every clip on one skeleton
   and writes `Assets/Anomaly/Gaunt/Rigged/SK_AH_Gaunt.fbx`.
6. Unreal: run `Tools/import_gaunt_ue58.py` again. It now also imports `SK_AH_Gaunt`, its
   skeleton and physics asset, and the clips as `Anims/A_Gaunt_Idle/Walk/Run/Attack/Death`.

From then on `AGauntAnomaly` uses the rigged one automatically: it picks Idle/Walk/Run from its
speed (`RunSpeedThreshold`), plays Attack when it strikes, Death then ragdoll when it dies.
A missing clip just falls back (Run -> Walk; no Death = straight to ragdoll).

`Assets/Anomaly/Gaunt/Mixamo/` is git-ignored (raw downloads); the merged `Rigged/` FBX is
what to commit.

## Status

Checked outside Unreal (Nov 2026): the stand-in mesh and cleaned textures were rendered in
Blender; the Mixamo merge script was tested on Mixamo-style test files (separate character and
"without skin" clips, Mixamo file names) and puts every clip on one skeleton. The C++ and the
Unreal import script have not been run in the engine yet.
