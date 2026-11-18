"""Anomaly Hunter / UE 5.8: import Meshy forest props tracked in Git LFS.

Source files: <repo>/Assets/MeshyForest/Meshy_AI_model*.fbx
Destination: /Game/ForestResearch/MeshyProps

Starts in TEST_MODE (one door and one sign). Assets are never overwritten.
Once visually checked, change TEST_MODE to False to import all 11.
Use Tools > Execute Python Script in Unreal Editor.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Assets" / "MeshyForest"
DEST = "/Game/ForestResearch/MeshyProps"
TEST_MODE = True

# Approximate identifications from source mesh silhouettes, not Meshy metadata.
# Confirm labels after previewing individual meshes in UE.
ASSETS = [
    ("Meshy_AI_model.fbx", "SM_FR_StorageCrate"),
    ("Meshy_AI_model(1).fbx", "SM_FR_ResearchWorkstation"),
    ("Meshy_AI_model(2).fbx", "SM_FR_IndustrialFixture"),
    ("Meshy_AI_model(3).fbx", "SM_FR_ExitFixture"),
    ("Meshy_AI_model(4).fbx", "SM_FR_BiohazardSign"),
    ("Meshy_AI_model(5).fbx", "SM_FR_LabDoor"),
    ("Meshy_AI_model(6).fbx", "SM_FR_Generator"),
    ("Meshy_AI_model(7).fbx", "SM_FR_RSUWallSign"),
    ("Meshy_AI_model(8).fbx", "SM_FR_IndustrialBarrier"),
    ("Meshy_AI_model(9).fbx", "SM_FR_FloorGrate"),
    ("Meshy_AI_model(10).fbx", "SM_FR_IndustrialShelf"),
]

lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(message):
    unreal.log("[AH MeshyForest] " + str(message))

def safe_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as error:
        unreal.log_warning("[AH MeshyForest] Could not set %s: %s" % (name, error))
        return False

def valid_source(path):
    if not path.is_file():
        unreal.log_warning("[AH MeshyForest] Missing %s. Did you copy it and run git lfs pull?" % path.name)
        return False
    with path.open("rb") as f:
        if f.read(55).startswith(b"version https://git-lfs.github.com/spec/v1"):
            unreal.log_error("[AH MeshyForest] %s is only a Git LFS pointer. Run 'git lfs pull' in Git Bash." % path.name)
            return False
    if path.stat().st_size < 10000:
        unreal.log_warning("[AH MeshyForest] %s is suspiciously small; verify the FBX download." % path.name)
        return False
    return True

def import_fbx(filename, name):
    dest = DEST + "/" + name
    if lib.does_asset_exist(dest):
        log("Kept existing asset: " + dest)
        return
    path = SOURCE / filename
    if not valid_source(path):
        return
    task = unreal.AssetImportTask()
    safe_prop(task, "filename", str(path))
    safe_prop(task, "destination_path", DEST)
    safe_prop(task, "destination_name", name)
    safe_prop(task, "replace_existing", False)
    safe_prop(task, "save", True)
    safe_prop(task, "automated", True)
    try:
        options = unreal.FbxImportUI()
        safe_prop(options, "import_as_skeletal", False)
        safe_prop(options, "import_mesh", True)
        safe_prop(options, "import_materials", True)
        safe_prop(options, "import_textures", True)
        safe_prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        mesh_options = options.get_editor_property("static_mesh_import_data")
        if mesh_options:
            # Meshy exports appear about 1.9 units across; test 100x for UE centimetres.
            # Check final dimensions before treating these as real-world-scaled props.
            safe_prop(mesh_options, "import_uniform_scale", 100.0)
            safe_prop(mesh_options, "auto_generate_collision", False)
            safe_prop(options, "static_mesh_import_data", mesh_options)
        safe_prop(task, "options", options)
    except Exception as error:
        unreal.log_warning("[AH MeshyForest] FBX options unavailable: %s; UE Interchange settings may apply." % error)

    log("Importing %s -> %s" % (filename, name))
    try:
        asset_tools.import_asset_tasks([task])
        asset = lib.load_asset(dest)
        if asset is None:
            unreal.log_warning("[AH MeshyForest] Import did not create expected asset: " + dest)
        elif asset.get_class().get_name() != "StaticMesh":
            unreal.log_warning("[AH MeshyForest] Imported an unexpected asset type at " + dest)
        else:
            log("Imported " + dest)
    except Exception as error:
        unreal.log_error("[AH MeshyForest] Failed %s: %s" % (name, error))

def main():
    if not SOURCE.is_dir():
        unreal.log_error("[AH MeshyForest] Folder missing: " + str(SOURCE))
        return
    if not lib.does_directory_exist(DEST):
        lib.make_directory(DEST)
    if TEST_MODE:
        selection = {"SM_FR_LabDoor", "SM_FR_BiohazardSign"}
        log("TEST_MODE: importing only the door and warning sign.")
    else:
        selection = {name for _, name in ASSETS}
        log("Full import selected. Some FBXs are millions of triangles.")
    for filename, name in ASSETS:
        if name in selection:
            import_fbx(filename, name)
    log("Done. Inspect orientation, dimensions, materials, Nanite and add simple collision before placing in a map.")
    if TEST_MODE:
        log("Once the first two are correct, set TEST_MODE=False and rerun.")

main()
