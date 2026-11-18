# Meshy Forest Facility: Placement Pass 01 (UE 5.8)

Adds `Tools/place_meshy_forest_props_ue58.py` to the existing Git branch.

## Read before running
- Your 11 Meshy meshes must already be imported into `/Game/ForestResearch/MeshyProps` (see `Tools/import_meshy_forest_ue58.py`).
- The source blockout map `/Game/Maps/AH_ForestResearch_Small` must exist **locally**. A Git pull does not necessarily contain locally generated `.umap` files.
- Imported models are very high-poly and their labels were inferred from preview silhouettes. Confirm the identities of props yourself.
- The script places **12 curated instances of 11 assets**. This is a layout preview, not a completed level.
- It will not edit or rename `AH_ForestResearch_Small`, `TestArena`, or the project game defaults.

## 1. Get the script (don't overwrite local Unreal work)
Save your level and close Unreal Editor. Then in Git Bash:

```bash
cd /c/UE5Projects/MyProject5
git status --short
git pull --ff-only origin feature/meshy-forest-props-gitlfs
```

If Git refuses because of local changes, STOP and protect your changes first. In particular, do **not** use `git reset --hard` or `git clean -fd` on this project.

## 2. Duplicate the working map
In the Unreal Content Browser:
1. Locate `Content/Maps/AH_ForestResearch_Small`.
2. Right-click **Duplicate**.
3. Rename the duplicate to `AH_ForestResearch_MeshyPass01`.
4. **Open the duplicated map** by double-clicking it.
5. Save once (Ctrl+S).

The script checks the active map name and refuses to run on a different level.

## 3. Run placement
Choose **Tools > Execute Python Script** and select:
`C:\UE5Projects\MyProject5\Tools\place_meshy_forest_props_ue58.py`

The script adds Meshy props in the planned lab, storage, generator, corridor, reception and entrance. It fits each mesh **uniformly** inside an approximate size envelope; this doesn't guarantee correct scale, facing or appearance.

For a few exact one-to-one placements, it hides corresponding placeholder blockout actors rather than destroying them. Existing walls, floors, lighting, level functionality and enemy AI remain unchanged.

Review the new placed actors using the World Outliner folder `AnomalyHunter/MeshyPass01`. All the new actor labels start with `AH_Meshy_`.

## 4. Verify and adjust
- Check that the doorway remains traversable: the door is **decorative and non-colliding** in this pass.
- Check that the workstation sits in the lab, the generator in the generator room, and the shelf in storage.
- Check mesh orientation, material slots, environmental lighting, textures, UVs and Nanite suitability.
- Avoid setting complex-as-simple collision for extremely dense assets. Add separate simple collision shapes for physical objects after their position is final.
- The script doesn't assign gameplay logic to the door, generator or equipment.

Save the **duplicate** map only after inspection using Ctrl+S. Test Play In Editor on the duplicate.

## 5. Re-run and rollback
Re-running the script in the same map skips already placed actor labels. To roll back completely, delete the duplicated `AH_ForestResearch_MeshyPass01` map **only if you've confirmed it has no unique desired work**. The original `AH_ForestResearch_Small` remains untouched.

## Implementation caveats
This script was committed through the GitHub connector, but cannot be executed or visually tested inside your local UE 5.8 Editor from here. Please send Output Log errors or a viewport screenshot if any props aren't visible or face the wrong direction.

Git does not automatically contain your generated levels or other locally modified `Content/` assets. This workflow uses the **local duplicate** so as not to overwrite or prematurely commit the user's existing Unreal content.
