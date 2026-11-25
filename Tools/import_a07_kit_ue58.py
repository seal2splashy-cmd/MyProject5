"""
import_a07_kit_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Imports the game-ready A-07 kit made by Tools/prepare_a07_kit_blender.py from your Meshy models:
  Assets/MeshyFacility/Kit/<Name>/SM_A07K_<Name>.fbx + T_A07K_<Name>_D.jpg / _N.png / _MR.jpg
into /Game/ForestResearch/A07Kit:
  SM_A07K_Vessel, SM_A07K_DeconPod, SM_A07K_Console, SM_A07K_VaultDoor, SM_A07K_WallPanel,
  SM_A07K_MountRing, SM_A07K_Catwalk   (+ M_A07K_<Name> materials, textures)

Real size, pivot at the bottom centre, front (the detailed side) facing +Y. Nanite on.
No collision on the meshes: the level script adds simple boxes (deck, rails, ramps, blockers).
Safe to re-run (re-imports in place). Then Tools/build_a07_kit_level_ue58.py.
"""

import os
import unreal

DST = "/Game/ForestResearch/A07Kit"
NAMES = ["Vessel", "DeconPod", "Console", "VaultDoor", "WallPanel", "MountRing", "Catwalk"]

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[A07 Kit import] " + str(msg))


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as exc:
        unreal.log_warning("[A07 Kit import] %s: %s" % (prop, exc))
        return False


def tools_dir():
    try:
        return os.path.dirname(os.path.abspath(__file__))
    except NameError:
        return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")


SRC = os.path.join(os.path.dirname(tools_dir()), "Assets", "MeshyFacility", "Kit")


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


def textures(name):
    dst = DST + "/Textures"
    folder(dst)
    out = {}
    for suffix, ext, srgb, comp in (("D", ".jpg", True, unreal.TextureCompressionSettings.TC_DEFAULT),
                                    ("N", ".png", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                                    ("MR", ".jpg", False, unreal.TextureCompressionSettings.TC_MASKS)):
        tname = "T_A07K_%s_%s" % (name, suffix)
        path = os.path.join(SRC, name, tname + ext)
        if not os.path.isfile(path):
            continue
        import_file(path, dst, tname)
        t = eal.load_asset(dst + "/" + tname)
        if t:
            t.set_editor_property("srgb", srgb)
            t.set_editor_property("compression_settings", comp)
            eal.save_loaded_asset(t)
            out[suffix] = t
    return out


def material(name, tex):
    dst = DST + "/Materials"
    folder(dst)
    mname = "M_A07K_" + name
    path = dst + "/" + mname
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(mname, dst, unreal.Material, unreal.MaterialFactoryNew())
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
    fbx = os.path.join(SRC, name, "SM_A07K_%s.fbx" % name)
    target = DST + "/SM_A07K_" + name
    tmp = DST + "/_Import_" + name
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
    try_set(sm, "build_nanite", True)
    try_set(sm, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    try_set(opts, "static_mesh_import_data", sm)
    import_file(fbx, tmp, "SM_A07K_" + name, opts)

    mesh = None
    for p in eal.list_assets(tmp, recursive=True, include_folder=False):
        obj = eal.load_asset(p)
        if isinstance(obj, unreal.StaticMesh):
            src = obj.get_path_name().split(".")[0]
            if eal.does_asset_exist(target):
                eal.delete_asset(target)
            eal.rename_asset(src, target)
            mesh = eal.load_asset(target)
            break
    for p in eal.list_assets(tmp, recursive=True, include_folder=False):
        eal.delete_asset(p.split(".")[0])
    eal.delete_directory(tmp)
    return mesh


def main():
    if not os.path.isdir(SRC):
        unreal.log_error("[A07 Kit import] %s not found - git pull (it's in Git LFS: git lfs pull)" % SRC)
        return
    folder(DST)
    done = []
    for name in NAMES:
        fbx = os.path.join(SRC, name, "SM_A07K_%s.fbx" % name)
        if not os.path.isfile(fbx):
            log("no %s, skipped" % fbx)
            continue
        with open(fbx, "rb") as f:
            if f.read(40).startswith(b"version https://git-lfs"):
                unreal.log_error("[A07 Kit import] %s is only a Git LFS pointer - run: git lfs pull" % fbx)
                continue
        mat = material(name, textures(name))
        mesh = static_mesh(name)
        if mesh is None:
            unreal.log_error("[A07 Kit import] %s imported but no static mesh turned up" % name)
            continue
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, mat)
        try:
            settings = mesh.get_editor_property("nanite_settings")
            settings.set_editor_property("enabled", True)
            mesh.set_editor_property("nanite_settings", settings)
        except Exception:
            pass
        eal.save_loaded_asset(mesh)
        done.append(name)
    log("imported %d/%d: %s. Next: open AH_ForestResearch_A07_Expanded, run Tools/build_a07_kit_level_ue58.py"
        % (len(done), len(NAMES), ", ".join(done)))


main()
