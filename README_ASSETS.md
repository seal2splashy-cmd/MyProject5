# Generated assets

Everything in `Assets/` is made by code, so it's free to use and easy to regenerate.

| Folder | What | Made by |
|---|---|---|
| `Assets/Audio` | 18 WAVs: melee whooshes, flesh/metal hits, gunshot, dry fire, reload, 4 footsteps, hurt, looping bodycam hiss | `Tools/generate_assets.py` |
| `Assets/Textures` | 8 tileable PNGs: concrete, rusted metal, wood, floor tiles (+ normals, rust roughness) | `Tools/generate_assets.py` |

## Getting them into Unreal (once)

1. Build the C++ and open the editor (the `.uproject` now enables the Python + Editor Scripting plugins; say yes if it asks to restart).
2. **Tools > Execute Python Script...** and pick `Tools/ue_setup_project.py`.
3. Wait for the progress bar. It:
   - imports the audio to `/Game/Generated/Audio` (the hiss is set to loop)
   - imports the textures to `/Game/Generated/Textures` (normals/roughness set to linear)
   - builds materials in `/Game/Generated/Materials` (world-aligned ones for walls/floors, UV ones for weapons)
   - builds `/Game/Maps/TestArena`: dark concrete warehouse, corridors, physics crates and barrels,
     a few dim coloured lights, bodycam post-process (locked exposure, grain, vignette, fringe),
     a player start and 4 enemies (2 smart, 2 basic)
4. Press Play. The code finds the sounds/materials by path automatically - nothing to wire up.

Re-running the script replaces everything it made (it only deletes actors tagged `GeneratedArena`,
so anything you add to the level by hand survives).

## Regenerating / tweaking

```
pip install numpy scipy pillow
python Tools/generate_assets.py
```
Then run the editor script again. All the sounds and textures are a handful of numbers each in
`generate_assets.py` - e.g. change `metal_clang`'s `base` frequency for a duller clang.

## Melee weapons

The pipe, crowbar, machete and fire axe are built in C++ (`MeleeWeaponTypes.cpp`) from the engine
cube and cylinder, so there's nothing to import and their orientation can't come out wrong.
They use `M_RustMetal` / `M_Wood` once the script has made them, flat tinted colours before that.
In game: **2** to draw melee, **2** again to cycle weapons.
