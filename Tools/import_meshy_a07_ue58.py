"""Import only the THREE supplied Meshy A-07 FBXs. UE 5.8 Editor script.

First run Tools/prepare_meshy_facility_pack.py against your downloaded ZIP,
commit/push via Git LFS or keep the extracted files locally.
The source meshes and textures are user-created Meshy exports; no 3D assets
are synthesized here.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Assets" / "MeshyFacility"
DEST = "/Game/ForestResearch/MeshyExpansion"

OBJECTS = [
    ("ContainmentVessel", "SM_A07_ContainmentVessel"),
    ("DeconChamber", "SM_A07_DeconChamber"),
    ("ControlConsole", "SM_A07_ControlConsole"),
]

def log(m): unreal.log("[A07 Meshy Import] " + str(m))
def warn(m): unreal.log_warning("[A07 Meshy Import] " + str(m))
def prop(o, n, v):
    try: o.set_editor_property(n, v)
    except Exception as e: warn("%s: %s" % (n, e))

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary

for folder, name in OBJECTS:
    directory = SOURCE / folder
    fbx = directory / "Meshy_AI_model.fbx"
    asset_dir = DEST + "/" + folder
    target = asset_dir + "/" + name
    if lib.does_asset_exist(target):
        log("Keeping existing " + target)
        continue
    if not fbx.is_file():
        warn("Missing " + str(fbx))
        continue
    with fbx.open("rb") as handle:
        if handle.read(55).startswith(b"version https://git-lfs.github.com/spec/v1"):
            warn("LFS pointer only, run 'git lfs pull': " + str(fbx))
            continue
    if not lib.does_directory_exist(asset_dir):
        lib.make_directory(asset_dir)
    task = unreal.AssetImportTask()
    prop(task, "filename", str(fbx))
    prop(task, "destination_path", asset_dir)
    prop(task, "destination_name", name)
    prop(task, "replace_existing", False)
    prop(task, "save", True)
    prop(task, "automated", True)
    try:
        opts = unreal.FbxImportUI()
        prop(opts, "import_as_skeletal", False)
        prop(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        prop(opts, "import_materials", True)
        prop(opts, "import_textures", True)
        data = opts.get_editor_property("static_mesh_import_data")
        if data:
            prop(data, "auto_generate_collision", False)
            # Keep native Meshy units here. The map builder fits mesh bounds in-place.
            prop(opts, "static_mesh_import_data", data)
        prop(task, "options", opts)
    except Exception as e:
        warn("UE 5.8 import UI options may differ: " + str(e))
    log("Importing " + str(fbx))
    try:
        tools.import_asset_tasks([task])
        if lib.does_asset_exist(target):
            log("Imported " + target)
        else:
            warn("Expected mesh not found: " + target + ". Check imported name in " + asset_dir)
    except Exception as e:
        unreal.log_error("[A07 Meshy Import] %s: %s" % (name, e))
log("Finished. Inspect model materials, size and performance in Static Mesh Editor.")
