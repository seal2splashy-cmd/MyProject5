"""Anomaly Hunter / UE 5.8: import Meshy forest props tracked in Git LFS.

Source files: <repo>/Assets/MeshyForest/Meshy_AI_model*.fbx
Destination: /Game/ForestResearch/MeshyProps

Starts in TEST_MODE (one door and one sign). Assets are never overwritten: if you ran an
earlier version of this script (it imported at x100 scale), delete /Game/ForestResearch/MeshyProps
first. Nanite is switched on for every mesh.
Once visually checked, change TEST_MODE to False to import all 11.
Use Tools > Execute Python Script in Unreal Editor.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Assets" / "MeshyForest"
DEST = "/Game/ForestResearch/MeshyProps"
TEST_MODE = True

# Checked by rendering every FBX (Docs/Preview/meshy_forest_props.jpg). Triangle counts are the
# raw Meshy output - every one of these needs Nanite.
ASSETS = [
    ("Meshy_AI_model.fbx",     "SM_FR_StorageCrate"),         # two stacked green ammo cases, 1.1M tris
    ("Meshy_AI_model(1).fbx",  "SM_FR_CeilingDuct"),          # ceiling vent duct + cable tray on hangers, 1.6M
    ("Meshy_AI_model(2).fbx",  "SM_FR_ResearchWorkstation"),  # lab desk: microscope, CRT terminal, stool, 0.9M
    ("Meshy_AI_model(3).fbx",  "SM_FR_ExitFixture"),          # wall-mounted green exit sign box, 0.4M
    ("Meshy_AI_model(4).fbx",  "SM_FR_BiohazardSign"),        # "Research area - biohazard - restricted", 0.4M
    ("Meshy_AI_model(5).fbx",  "SM_FR_LabDoor"),              # steel door in frame, window + keypad, 0.7M
    ("Meshy_AI_model(6).fbx",  "SM_FR_RSUWallPanel"),         # flat concrete wall panel with the RSU logo, 0.1M
    ("Meshy_AI_model(7).fbx",  "SM_FR_Generator"),            # yellow GEN-03 diesel generator, 3.5M
    ("Meshy_AI_model(8).fbx",  "SM_FR_HangingLight"),         # industrial light fitting on two chains, 0.7M
    ("Meshy_AI_model(9).fbx",  "SM_FR_FloorGrate"),           # square drain grate, 0.15M
    ("Meshy_AI_model(10).fbx", "SM_FR_IndustrialShelf"),      # steel shelving with boxes and canisters, 3.8M
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

def enable_nanite(mesh):
    """Millions of triangles each: without Nanite a few of these would sink the frame rate."""
    try:
        settings = mesh.get_editor_property("nanite_settings")
        if not settings.get_editor_property("enabled"):
            settings.set_editor_property("enabled", True)
            mesh.set_editor_property("nanite_settings", settings)
            lib.save_loaded_asset(mesh)
    except Exception as error:
        unreal.log_warning("[AH MeshyForest] Couldn't switch Nanite on for %s (%s) - tick it in the mesh editor"
                           % (mesh.get_name(), error))

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
            # The FBXs declare centimetres and come in at ~190 cm (Meshy scales every model so its
            # longest side is ~1.9 m). Keep 1.0: x100 would make each asset ~190 m. Real sizes are
            # set per placement by Tools/place_meshy_forest_props_ue58.py.
            safe_prop(mesh_options, "import_uniform_scale", 1.0)
            safe_prop(mesh_options, "auto_generate_collision", False)
            safe_prop(mesh_options, "build_nanite", True)
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
            enable_nanite(asset)
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
