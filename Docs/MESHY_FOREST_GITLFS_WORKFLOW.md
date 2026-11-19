# Meshy Forest Props via Git LFS (Unreal Engine 5.8)

This branch adds **Git tracking rules, an Unreal import helper and a placement pass** for the 11 forest-facility FBX assets. The FBX files themselves are **on GitHub through Git LFS** (pushed 18 Nov 2026), so a `git pull` with Git LFS installed fetches them. Steps 2-3 are only needed if you add or replace models.

## What is committed on this branch
- `.gitattributes`: Git LFS for `Assets/MeshyForest/*.fbx` and optionally generated `Content/ForestResearch/MeshyProps/*.uasset`.
- `Assets/MeshyForest/*.fbx`: the 11 models (~790 MiB, in LFS).
- `Tools/import_meshy_forest_ue58.py`: imports the two test assets first, then all eleven after review. Refuses to overwrite existing Unreal assets, detects un-fetched LFS pointer files, imports at real Meshy size (scale 1.0) and switches Nanite on.
- `Tools/place_meshy_forest_props_ue58.py`: dresses a duplicate of the forest map (Docs/MESHY_FOREST_PLACEMENT_PASS01.md).

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

They appear under `Content/ForestResearch/MeshyProps/`, about 190 cm on their longest side (Meshy normalises every model to that; the placement script sets real sizes). If you imported with an earlier version of this script (x100 scale), delete that folder first. Check material slots, normals and that Nanite is on; use simple collision, not complex per-triangle collision.

After those two pass, edit `TEST_MODE = False` in the importer, save, and rerun to import the remaining nine. The script skips any existing assets.

## Step 5: Arrange in the new map
Open `Content/Maps/AH_ForestResearch_Small` (run `Tools/build_forest_research_ue58.py` once if the map isn't present). Place or replace modular props manually, validating door clearance and collisions. Do not overwrite the existing TestArena.

## Important limitations
- Every `git clone` / `git lfs pull` downloads the full ~790 MiB and counts against your GitHub LFS bandwidth.
- Existing `.uasset` and `.umap` files on your PC are not automatically present on GitHub: commit explicitly if you want them versioned.
- The 11 names were checked by rendering every model (Docs/Preview/meshy_forest_props.jpg). The first version of the importer had five of them wrong.
- The importer and its UE 5.8 import options have **not been tested inside Unreal Engine here**. Some 5.8 Interchange workflows may require reviewing import settings.
- A single static mesh can contain millions of polygons; do not populate large numbers of these without evaluating GPU/CPU memory and collision performance.
