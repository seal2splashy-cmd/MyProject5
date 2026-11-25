"""
import_meshy_weapons_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Imports the Meshy weapons prepared by Tools/prepare_meshy_weapons_blender.py:
  Assets/Weapons/SM_AH_Pistol.fbx + T_AH_Pistol_D/N/MR  ->  /Game/Generated/Weapons/SM_AH_Pistol  (AGun)
  Assets/Weapons/SM_AH_Pipe.fbx   + T_AH_Pipe_D/N/MR    ->  /Game/Generated/Weapons/SM_AH_Pipe    (AMeleePipe)
with a material each (M_AH_Pistol / M_AH_Pipe: colour, DirectX normal, roughness = G and
metallic = B of the MR map). The game uses them automatically on the next Play.

Safe to re-run: re-imports and rebuilds in place. Skips any weapon whose files aren't there.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT_DIR, "Assets", "Weapons")
DST = "/Game/Generated/Weapons"
WEAPONS = ("Pistol", "Pipe")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Meshy weapons] " + msg)


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
    except Exception:
        pass   # legacy FBX option missing in this engine version (Interchange handles FBX)


def folder(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def import_file(path, dest, name=None, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    if name:
        task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    if options:
        task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])


def move_to(obj, target):
    src = obj.get_path_name().split(".")[0]
    if src != target:
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        eal.rename_asset(src, target)
    return eal.load_asset(target)


def textures(name):
    tex_dst = DST + "/Textures"
    folder(tex_dst)
    out = {}
    for suffix, srgb, comp in (("D", True, unreal.TextureCompressionSettings.TC_DEFAULT),
                               ("N", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                               ("MR", False, unreal.TextureCompressionSettings.TC_MASKS)):
        tname = "T_AH_%s_%s" % (name, suffix)
        path = os.path.join(SRC, tname + ".png")
        if not os.path.isfile(path):
            continue
        import_file(path, tex_dst, tname)
        t = eal.load_asset(tex_dst + "/" + tname)
        if t:
            t.set_editor_property("srgb", srgb)
            t.set_editor_property("compression_settings", comp)
            eal.save_loaded_asset(t)
            out[suffix] = t
    return out


def material(name, tex):
    mat_dst = DST + "/Materials"
    folder(mat_dst)
    mname = "M_AH_" + name
    path = mat_dst + "/" + mname
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)      # rebuild in place, keeps references
    else:
        if mat is not None:
            eal.delete_asset(path)
        mat = asset_tools.create_asset(mname, mat_dst, unreal.Material, unreal.MaterialFactoryNew())

    if "D" in tex:
        d = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -200)
        d.set_editor_property("texture", tex["D"])
        mel.connect_material_property(d, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if "N" in tex:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 100)
        n.set_editor_property("texture", tex["N"])
        try_set(n, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if "MR" in tex:
        mr = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 400)
        mr.set_editor_property("texture", tex["MR"])
        try_set(mr, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mel.connect_material_property(mr, "G", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(mr, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def static_mesh(name):
    fbx = os.path.join(SRC, "SM_AH_%s.fbx" % name)
    target = DST + "/SM_AH_" + name
    tmp = DST + "/_Import_" + name       # own folder, so nothing else in Weapons gets picked up
    folder(tmp)
    opts = unreal.FbxImportUI()
    try_set(opts, "import_mesh", True)
    try_set(opts, "import_as_skeletal", False)
    try_set(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    try_set(opts, "import_materials", False)
    try_set(opts, "import_textures", False)
    sm = opts.get_editor_property("static_mesh_import_data")
    try_set(sm, "combine_meshes", True)
    try_set(sm, "generate_lightmap_u_vs", True)
    try_set(sm, "auto_generate_collision", False)
    try_set(sm, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    try_set(opts, "static_mesh_import_data", sm)
    import_file(fbx, tmp, "SM_AH_" + name, opts)

    mesh = None
    for p in eal.list_assets(tmp, recursive=True, include_folder=False):
        obj = eal.load_asset(p)
        if isinstance(obj, unreal.StaticMesh):
            mesh = move_to(obj, target)
            break
    for p in eal.list_assets(tmp, recursive=True, include_folder=False):
        eal.delete_asset(p.split(".")[0])         # whatever else the importer made
    eal.delete_directory(tmp)
    return mesh


def main():
    folder(DST)
    done = []
    for name in WEAPONS:
        if not os.path.isfile(os.path.join(SRC, "SM_AH_%s.fbx" % name)):
            log("no SM_AH_%s.fbx, skipped" % name)
            continue
        mat = material(name, textures(name))
        mesh = static_mesh(name)
        if mesh is None:
            unreal.log_error("[Meshy weapons] %s imported but no static mesh turned up" % name)
            continue
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, mat)
        eal.save_loaded_asset(mesh)
        done.append(name)
    log("imported: %s - press Play (1 = pistol, 2 = melee, the pipe is first)" % (", ".join(done) or "nothing"))


main()
