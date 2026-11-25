"""Import four user-created Meshy A-07 modules in Unreal Engine 5.8.

Run in Unreal Editor: Tools > Execute Python Script.
First stage ZIPs via Tools/prepare_a07_meshy_modules.py and git lfs pull.

Never overwrites assets. Original texture maps remain in source folders.
Because FBX/PBR material auto-import depends on UE's importer version,
inspect material slots and connect normal/roughness/metallic manually if needed.
"""
from pathlib import Path
import unreal

SOURCE = Path(__file__).resolve().parent.parent / "Assets" / "MeshyFacility"
TARGET = "/Game/ForestResearch/MeshyExpansion/Modules"
MODULES = [
    ("MountingPlatform", "SM_A07_MountingPlatform"),
    ("Catwalk", "SM_A07_Catwalk"),
    ("DamagedWall", "SM_A07_DamagedWall"),
    ("ContainmentEntrance", "SM_A07_ContainmentEntrance"),
]
lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(message):
    unreal.log("[A07 Modules] " + message)

def warn(message):
    unreal.log_warning("[A07 Modules] " + message)

def setprop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as err:
        warn("Couldn't set %s (%s)" % (name, err))

def main():
    for folder, name in MODULES:
        source_dir = SOURCE / folder
        if not source_dir.is_dir():
            warn("Source folder missing: " + str(source_dir))
            continue
        fbxs = list(source_dir.glob("*.fbx"))
        if len(fbxs) != 1:
            warn("Expected one FBX in %s, found %d" % (source_dir, len(fbxs)))
            continue
        fbx = fbxs[0]
        with fbx.open("rb") as src:
            if src.read(55).startswith(b"version https://git-lfs.github.com/spec/v1"):
                warn("Git LFS pointer, run git lfs pull: " + str(fbx))
                continue
        dest_dir = TARGET + "/" + folder
        dest_asset = dest_dir + "/" + name
        if lib.does_asset_exist(dest_asset):
            log("Already exists, keeping: " + dest_asset)
            continue
        lib.make_directory(dest_dir)
        task = unreal.AssetImportTask()
        setprop(task, "filename", str(fbx))
        setprop(task, "destination_path", dest_dir)
        setprop(task, "destination_name", name)
        setprop(task, "replace_existing", False)
        setprop(task, "automated", True)
        setprop(task, "save", True)
        try:
            options = unreal.FbxImportUI()
            setprop(options, "import_as_skeletal", False)
            setprop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
            setprop(options, "import_materials", True)
            setprop(options, "import_textures", True)
            static_data = options.get_editor_property("static_mesh_import_data")
            if static_data:
                setprop(static_data, "auto_generate_collision", False)
            setprop(task, "options", options)
        except Exception as error:
            warn("FBX UI settings unavailable; check import behaviour: " + str(error))
        try:
            log("Importing %s..." % fbx.name)
            asset_tools.import_asset_tasks([task])
            imported = lib.load_asset(dest_asset)
            if imported and isinstance(imported, unreal.StaticMesh):
                log("Imported " + dest_asset)
                try:
                    box = imported.get_bounding_box()
                    log("Mesh bounds (asset units): min=%s max=%s" % (box.min, box.max))
                except Exception:
                    pass
            else:
                warn("Expected imported StaticMesh not found: " + dest_asset)
        except Exception as error:
            unreal.log_error("[A07 Modules] Failed %s: %s" % (name, error))
    log("Finished. Inspect material channels, scale, normals and collision before map placement.")

main()
