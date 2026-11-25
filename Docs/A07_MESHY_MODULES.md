# A-07 containment chamber: four Meshy-supplied modules

User-provided ZIPs (25 November 2026):

| ZIP | Contents | Target Unreal asset |
|---|---|---|
| containment_mounting_.zip | 1 FBX + 4 PBR PNGs | SM_A07_MountingPlatform |
| elevated_catwalk_sect.zip | 1 FBX + 4 PBR PNGs | SM_A07_Catwalk |
| damaged_concrete_wall.zip | 1 FBX + 4 PBR PNGs | SM_A07_DamagedWall |
| containment_entrance_.zip | 1 FBX + 4 PBR PNGs | SM_A07_ContainmentEntrance |

These are **original Meshy assets from the user's uploads**, not AI-generated substitutes. ZIPs contain base-colour, normal, roughness, metallic textures for each FBX.

## 1. Pull code (leave local Unreal Content and modified configs alone)

Save and close UE, then in Git Bash:

\`\`\`bash
cd /c/UE5Projects/MyProject5
git status --short
git pull --ff-only origin feature/meshy-forest-props-gitlfs
\`\`\`

If pull refuses because of local changes, stop to preserve work. Do not run \`git reset --hard\`, \`git clean -fd\` or \`git add .\`.

## 2. Stage files from four ZIPs

Download all four ZIPs to Windows Downloads (or any one directory). In PowerShell or Git Bash:

\`\`\`bash
python Tools/prepare_a07_meshy_modules.py "C:/Users/YOUR_USERNAME/Downloads" --check
python Tools/prepare_a07_meshy_modules.py "C:/Users/YOUR_USERNAME/Downloads"
\`\`\`

Replace \`YOUR_USERNAME\` or pass the actual folder. Script verifies exactly one FBX and four PNGs in each ZIP, preserves source file names and refuses to replace different files. It copies originals into separate:

\`\`\`text
Assets/MeshyFacility/MountingPlatform/
Assets/MeshyFacility/Catwalk/
Assets/MeshyFacility/DamagedWall/
Assets/MeshyFacility/ContainmentEntrance/
\`\`\`

## 3. Check Git LFS and push the art

\`\`\`bash
git check-attr filter -- Assets/MeshyFacility/MountingPlatform/*.fbx
git add Assets/MeshyFacility/MountingPlatform Assets/MeshyFacility/Catwalk Assets/MeshyFacility/DamagedWall Assets/MeshyFacility/ContainmentEntrance
git lfs ls-files
git diff --cached --name-only
git commit -m "Add A-07 Meshy modular chamber assets and PBR textures"
git push origin feature/meshy-forest-props-gitlfs
\`\`\`

These four ZIPs total ~530 MiB; individual FBXs exceed the ordinary 100 MB GitHub limit and must be tracked via Git LFS. Check quota. The output should show **20 newly staged files**, provided those folders were previously absent. Avoid staging Content, project config or temporary files.

## 4. Import in Unreal

Open UE 5.8. **Tools → Execute Python Script →**
\`C:\\UE5Projects\\MyProject5\\Tools\\import_a07_meshy_modules_ue58.py\`

The import creates:
- \`/Game/ForestResearch/MeshyExpansion/Modules/MountingPlatform/SM_A07_MountingPlatform\`
- \`/Game/ForestResearch/MeshyExpansion/Modules/Catwalk/SM_A07_Catwalk\`
- \`/Game/ForestResearch/MeshyExpansion/Modules/DamagedWall/SM_A07_DamagedWall\`
- \`/Game/ForestResearch/MeshyExpansion/Modules/ContainmentEntrance/SM_A07_ContainmentEntrance\`

Nothing existing is replaced. Importing may take time because these are large assets. Inspect texture wiring (base colour, normal, roughness, metallic), Nanite, face normals and scale; the importer does not create game logic or finished collision. **Never use complex-as-simple collision on very dense meshes for gameplay.**

## 5. Layout planning (not yet placed automatically)

Keep original \`AH_ForestResearch_Small\` and \`TestArena\` untouched. Duplicate or create a **separate** A-07 expansion map. Start with the already imported Meshy vessel, place the mounting platform beneath it, connect catwalks with appropriate clearances, position the entrance at the chamber opening, and use the damaged-wall asset sparingly for visual storytelling. Unreal blockout floors/walls with simple collision should remain authoritative for navigation.

This source branch includes earlier draft mission/controller classes, but this stage does **not** claim a functional tested level or finished mission. Local UE validation is required, especially before replacing original meshes.

## Source archive notes

The ZIPs contain one FBX and four PNGs each, using Meshy's timestamped filenames. The FBX files are approximately 143 MB, 107 MB, 60 MB and 48 MB, respectively. The original ZIPs are not committed to Git; stage their extracted FBX and PNG components using Git LFS to avoid storing duplicates.
