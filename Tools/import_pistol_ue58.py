"""
import_pistol_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Imports Assets/Weapons/SM_AH_ServicePistol.fbx (made by Tools/build_pistol_blender.py) as
/Game/Generated/Weapons/SM_AH_ServicePistol, builds its five materials in
/Game/Generated/Weapons/Materials and assigns them by slot name. AGun picks the mesh up
automatically on the next Play (it's preferred over the field sidearm and the old Fab pistol).

Safe to re-run: re-imports the mesh and rebuilds the materials in place. Touches nothing else.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT_DIR, "Assets", "Weapons", "SM_AH_ServicePistol.fbx")
DST = "/Game/Generated/Weapons"
MAT_DST = DST + "/Materials"
NAME = "SM_AH_ServicePistol"
TARGET = DST + "/" + NAME

# slot name: (base colour (linear), metallic, roughness, emissive colour or None)
MATERIALS = {
    "M_Pistol_Slide": ((0.035, 0.037, 0.040), 0.85, 0.38, None),   # dark gunmetal
    "M_Pistol_Frame": ((0.022, 0.022, 0.024), 0.0, 0.62, None),    # black polymer
    "M_Pistol_Grip":  ((0.016, 0.016, 0.018), 0.0, 0.92, None),    # stippled panels
    "M_Pistol_Steel": ((0.20, 0.20, 0.21), 1.0, 0.30, None),       # barrel, levers
    "M_Pistol_Dots":  ((0.75, 0.95, 0.80), 0.0, 0.40, (0.5, 2.0, 0.9)),  # glowing sight dots
}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Pistol] " + msg)


def warn(msg):
    unreal.log_warning("[Pistol] " + msg)


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
    except Exception:
        pass   # legacy FBX option missing in this engine version (Interchange handles FBX)


def import_mesh():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SRC)
    task.set_editor_property("destination_path", DST)
    task.set_editor_property("destination_name", NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    opts = unreal.FbxImportUI()
    try_set(opts, "import_mesh", True)
    try_set(opts, "import_as_skeletal", False)
    try_set(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    try_set(opts, "import_materials", False)
    try_set(opts, "import_textures", False)
    try_set(opts, "import_animations", False)
    sm = opts.get_editor_property("static_mesh_import_data")
    try_set(sm, "combine_meshes", True)
    try_set(sm, "generate_lightmap_u_vs", True)
    try_set(sm, "auto_generate_collision", False)
    try_set(sm, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    try_set(opts, "static_mesh_import_data", sm)
    task.set_editor_property("options", opts)
    asset_tools.import_asset_tasks([task])

    mesh = eal.load_asset(TARGET) if eal.does_asset_exist(TARGET) else None
    if isinstance(mesh, unreal.StaticMesh):
        return mesh
    # Interchange may name the asset after the FBX node; find it and move it into place.
    for path in eal.list_assets(DST, recursive=False, include_folder=False):
        obj = eal.load_asset(path)
        if isinstance(obj, unreal.StaticMesh):
            src = obj.get_path_name().split(".")[0]
            if src != TARGET:
                if eal.does_asset_exist(TARGET):
                    eal.delete_asset(TARGET)
                eal.rename_asset(src, TARGET)
            return eal.load_asset(TARGET)
    return None


def build_material(name, colour, metallic, roughness, emissive):
    path = MAT_DST + "/" + name
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)      # rebuild in place, keeps references
    else:
        if mat is not None:
            eal.delete_asset(path)
        mat = asset_tools.create_asset(name, MAT_DST, unreal.Material, unreal.MaterialFactoryNew())

    c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, -100)
    c.set_editor_property("constant", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    m = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 60)
    m.set_editor_property("r", metallic)
    mel.connect_material_property(m, "", unreal.MaterialProperty.MP_METALLIC)
    r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 160)
    r.set_editor_property("r", roughness)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        e = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 260)
        e.set_editor_property("constant", unreal.LinearColor(emissive[0], emissive[1], emissive[2], 1.0))
        mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def main():
    if not os.path.isfile(SRC):
        unreal.log_error("[Pistol] missing %s - run Tools/build_pistol_blender.py in Blender first" % SRC)
        return
    for folder in (DST, MAT_DST):
        if not eal.does_directory_exist(folder):
            eal.make_directory(folder)

    mesh = import_mesh()
    if mesh is None:
        unreal.log_error("[Pistol] the FBX imported but no static mesh turned up in " + DST)
        return

    mats = {name: build_material(name, *spec) for name, spec in MATERIALS.items()}

    slots = mesh.get_editor_property("static_materials")
    assigned = []
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        key = next((k for k in mats if slot_name == k or slot_name.startswith(k)), None)
        if key:
            mesh.set_material(i, mats[key])
            assigned.append(key)
        else:
            warn("no material for slot '%s'" % slot_name)
    eal.save_loaded_asset(mesh)

    # Interchange may also have made its own material instances next to the mesh: unused now.
    for path in eal.list_assets(DST, recursive=False, include_folder=False):
        if path.split(".")[0] == TARGET:
            continue
        obj = eal.load_asset(path)
        if isinstance(obj, (unreal.MaterialInterface, unreal.Texture)):
            eal.delete_asset(path.split(".")[0])

    log("imported %s, materials on %d slots: %s" % (TARGET, len(assigned), ", ".join(assigned)))
    log("press Play - AGun uses this pistol automatically.")


main()
