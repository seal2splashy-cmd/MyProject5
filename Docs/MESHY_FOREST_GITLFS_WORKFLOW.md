# Meshy Forest Props via Git LFS (Unreal Engine 5.8)

This branch adds **Git tracking rules and an Unreal import helper** for the 11 forest-facility FBX assets. The large FBX binaries are **not yet in GitHub**: they must be added and pushed from the machine holding the downloaded Meshy originals.

## What is committed on this branch
- `.gitattributes`: Git LFS for `Assets/MeshyForest/*.fbx` and optionally generated `Content/ForestResearch/MeshyProps/*.uasset`.
- `Tools/import_meshy_forest_ue58.py`: imports the two test assets first, then all eleven after review. Refuses to overwrite existing Unreal assets and detects un-fetched LFS pointer files.

## Why Git LFS?
The eleven FBX files total about **790 MiB**, with individual files above 100 MiB. Normal Git/GitHub file uploads are not appropriate for these. Git LFS stores references in Git and transfers original data separately. Check your account's Git LFS storage/bandwidth quota before pushing.

## Step 1: Switch branches safely
Close Unreal and Visual Studio first. In Git Bash:

```bash
cd /c/UE5Projects/MyProject5
git status --short
git fetch origin
git switch --track origin/feature/meshy-forest-props-gitlfs
```

If `git status --short` is not empty, STOP and save/commit/stash your work before switching. If the branch already exists locally: `git switch feature/meshy-forest-props-gitlfs && git pull --ff-only`.

## Step 2: Set up Git LFS and copy the assets
Install Git LFS if not already present. Then:

```bash
git lfs install
git lfs version
mkdir -p Assets/MeshyForest
```

Copy **all 11 original downloaded Meshy .fbx files** into `C:\UE5Projects\MyProject5\Assets\MeshyForest\`. Keep the original names EXACTLY:

```text
Meshy_AI_model.fbx
Meshy_AI_model(1).fbx
Meshy_AI_model(2).fbx
Meshy_AI_model(3).fbx
Meshy_AI_model(4).fbx
Meshy_AI_model(5).fbx
Meshy_AI_model(6).fbx
Meshy_AI_model(7).fbx
Meshy_AI_model(8).fbx
Meshy_AI_model(9).fbx
Meshy_AI_model(10).fbx
```

## Step 3: Stage and push via Git LFS
```bash
git check-attr filter -- Assets/MeshyForest/Meshy_AI_model.fbx
git add Assets/MeshyForest
git lfs ls-files
git status --short
git commit -m "Add Meshy forest facility FBX assets via Git LFS"
git push -u origin feature/meshy-forest-props-gitlfs
```

**Check:** the first command should print `filter: lfs`, and `git lfs ls-files` should include 11 filenames. If not, do not commit/push. If Git reports an LFS quota error, do not bypass it by pushing raw huge FBXs. Use smaller/decimated models or a different permitted asset store.

## Step 4: Import in UE 5.8
If your LFS download is incomplete, run `git lfs pull` from the project root.

Open `MyProject5.uproject`. Choose **Tools > Execute Python Script** and select `Tools/import_meshy_forest_ue58.py`. It starts with `TEST_MODE=True` and imports:
- `SM_FR_LabDoor` from `Meshy_AI_model(5).fbx`
- `SM_FR_BiohazardSign` from `Meshy_AI_model(4).fbx`

They appear under `Content/ForestResearch/MeshyProps/`. Verify scale, material slots, normals, UVs, polygon counts and collision. Current FBXs are extremely high-poly, so **enable Nanite for suitable static meshes** and use simple collision, not complex per-triangle collision for gameplay. Imported PBR materials may require manual adjustment.

After those two pass, edit `TEST_MODE = False` in the importer, save, and rerun to import the remaining nine. The script skips any existing assets.

## Step 5: Arrange in the new map
Open `Content/Maps/AH_ForestResearch_Small` (run `Tools/build_forest_research_ue58.py` once if the map isn't present). Place or replace modular props manually, validating door clearance and collisions. Do not overwrite the existing TestArena.

## Important limitations
- Git doesn't run Blender/Unreal for you. This branch handles source delivery and import script versioning, not automatic 3D world placement.
- Existing `.uasset` and `.umap` files on your PC are not automatically present on GitHub: commit explicitly if you want them versioned.
- The 11 names were inferred from preview silhouettes. Confirm which object each actually is before positioning it in the map.
- The importer and its UE 5.8 import options have **not been tested inside Unreal Engine here**. Some 5.8 Interchange workflows may require reviewing import settings.
- A single static mesh can contain millions of polygons; do not populate large numbers of these without evaluating GPU/CPU memory and collision performance.
