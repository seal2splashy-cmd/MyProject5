"""
import_gaunt_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Brings the Gaunt (the forest anomaly) into /Game/Generated/Anomaly/Gaunt for AGauntAnomaly:

  Always:  T_AH_Gaunt_D/N/R textures, material M_AH_Gaunt, and the stand-in static mesh
           SM_AH_Gaunt (from Assets/Anomaly/Gaunt).
  Rigged:  if Assets/Anomaly/Gaunt/Rigged/SK_AH_Gaunt.fbx exists (made from Mixamo downloads by
           Tools/rig_gaunt_from_mixamo_blender.py): the skeletal mesh SK_AH_Gaunt, its skeleton,
           physics asset (ragdoll), and the clips as Anims/A_Gaunt_Idle/Walk/Run/Attack/Death.

AGauntAnomaly uses the rigged version as soon as it exists, otherwise the stand-in.
Safe to re-run: re-imports and rebuilds in place. Touches nothing outside that folder.
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT_DIR, "Assets", "Anomaly", "Gaunt")
RIGGED_FBX = os.path.join(SRC, "Rigged", "SK_AH_Gaunt.fbx")

DST = "/Game/Generated/Anomaly/Gaunt"
TEX_DST = DST + "/Textures"
ANIM_DST = DST + "/Anims"
RIG_TMP = DST + "/RigImport"           # rigged FBX lands here first, then gets sorted
CLIPS = ("Idle", "Walk", "Run", "Attack", "Death")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Gaunt] " + msg)


def warn(msg):
    unreal.log_warning("[Gaunt] " + msg)


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


def assets_in(path, cls):
    out = []
    for p in eal.list_assets(path, recursive=False, include_folder=False):
        obj = eal.load_asset(p)
        if isinstance(obj, cls):
            out.append(obj)
    return out


def move_to(obj, target):
    src = obj.get_path_name().split(".")[0]
    if src == target:
        return eal.load_asset(target)
    if eal.does_asset_exist(target):
        eal.delete_asset(target)
    eal.rename_asset(src, target)
    return eal.load_asset(target)


# ---------------------------------------------------------------- textures + material

def import_textures():
    folder(TEX_DST)
    tex = {}
    for suffix, srgb, comp in (("D", True, unreal.TextureCompressionSettings.TC_DEFAULT),
                               ("N", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                               ("R", False, unreal.TextureCompressionSettings.TC_MASKS)):
        name = "T_AH_Gaunt_" + suffix
        path = os.path.join(SRC, name + ".png")
        if not os.path.isfile(path):
            warn("missing " + path)
            continue
        import_file(path, TEX_DST, name)
        t = eal.load_asset(TEX_DST + "/" + name)
        if t:
            t.set_editor_property("srgb", srgb)
            t.set_editor_property("compression_settings", comp)
            eal.save_loaded_asset(t)
            tex[suffix] = t
    return tex


def build_material(tex):
    name, path = "M_AH_Gaunt", DST + "/M_AH_Gaunt"
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)
    else:
        if mat is not None:
            eal.delete_asset(path)
        mat = asset_tools.create_asset(name, DST, unreal.Material, unreal.MaterialFactoryNew())

    if "D" in tex:
        d = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -200)
        d.set_editor_property("texture", tex["D"])
        mel.connect_material_property(d, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if "N" in tex:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 100)
        n.set_editor_property("texture", tex["N"])
        try_set(n, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if "R" in tex:
        r = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 400)
        r.set_editor_property("texture", tex["R"])
        try_set(r, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mel.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 400)
        c.set_editor_property("r", 0.75)
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


# ---------------------------------------------------------------- stand-in

def import_stand_in(mat):
    target = DST + "/SM_AH_Gaunt"
    opts = unreal.FbxImportUI()
    try_set(opts, "import_mesh", True)
    try_set(opts, "import_as_skeletal", False)
    try_set(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    try_set(opts, "import_materials", False)
    try_set(opts, "import_textures", False)
    sm = opts.get_editor_property("static_mesh_import_data")
    try_set(sm, "combine_meshes", True)
    try_set(sm, "generate_lightmap_u_vs", True)
    try_set(sm, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    try_set(opts, "static_mesh_import_data", sm)
    import_file(os.path.join(SRC, "SM_AH_Gaunt.fbx"), DST, "SM_AH_Gaunt", opts)

    mesh = eal.load_asset(target) if eal.does_asset_exist(target) else None
    if not isinstance(mesh, unreal.StaticMesh):
        found = assets_in(DST, unreal.StaticMesh)
        mesh = move_to(found[0], target) if found else None
    if mesh is None:
        unreal.log_error("[Gaunt] stand-in FBX imported but no static mesh turned up in " + DST)
        return None
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, mat)
    eal.save_loaded_asset(mesh)
    log("stand-in " + target)
    return mesh


# ---------------------------------------------------------------- rigged (after Mixamo)

def import_rigged(mat):
    folder(RIG_TMP)
    opts = unreal.FbxImportUI()
    try_set(opts, "import_mesh", True)
    try_set(opts, "import_as_skeletal", True)
    try_set(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    try_set(opts, "import_animations", True)
    try_set(opts, "import_materials", False)
    try_set(opts, "import_textures", False)
    try_set(opts, "create_physics_asset", True)
    import_file(RIGGED_FBX, RIG_TMP, None, opts)

    meshes = assets_in(RIG_TMP, unreal.SkeletalMesh) + assets_in(DST, unreal.SkeletalMesh)
    if not meshes:
        unreal.log_error("[Gaunt] rigged FBX imported but no skeletal mesh turned up")
        return
    sk = move_to(meshes[0], DST + "/SK_AH_Gaunt")

    # Skeleton and physics asset next to it, with tidy names.
    for cls, name in ((unreal.Skeleton, "SKEL_AH_Gaunt"), (unreal.PhysicsAsset, "PHYS_AH_Gaunt")):
        found = assets_in(RIG_TMP, cls)
        if found:
            move_to(found[0], DST + "/" + name)

    # Our material on every slot.
    try:
        slots = sk.get_editor_property("materials")
        for s in slots:
            s.set_editor_property("material_interface", mat)
        sk.set_editor_property("materials", slots)
    except Exception as e:
        warn("couldn't set the skeletal mesh materials (%s) - assign M_AH_Gaunt by hand" % e)
    eal.save_loaded_asset(sk)

    # Clips: named after the Blender actions (Idle, Walk, ...) somewhere in the asset name.
    folder(ANIM_DST)
    named = []
    for anim in assets_in(RIG_TMP, unreal.AnimSequence):
        n = anim.get_name().lower()
        clip = next((c for c in CLIPS if c.lower() in n), None)
        if clip:
            move_to(anim, ANIM_DST + "/A_Gaunt_" + clip)
            named.append(clip)
        else:
            move_to(anim, ANIM_DST + "/" + anim.get_name())   # extra clips: kept as they are

    # Whatever else the importer made (its own materials/textures) isn't used.
    for p in eal.list_assets(RIG_TMP, recursive=True, include_folder=False):
        eal.delete_asset(p.split(".")[0])
    eal.delete_directory(RIG_TMP)

    log("rigged %s/SK_AH_Gaunt, clips: %s" % (DST, ", ".join(named) or "none found"))
    missing = [c for c in CLIPS if c not in named]
    if missing:
        warn("no clip for: " + ", ".join(missing))


def main():
    folder(DST)
    tex = import_textures()
    mat = build_material(tex)
    import_stand_in(mat)
    if os.path.isfile(RIGGED_FBX):
        import_rigged(mat)
    else:
        log("no rigged FBX yet (Assets/Anomaly/Gaunt/Rigged/SK_AH_Gaunt.fbx) - using the stand-in. "
            "See Docs/GAUNT_ANOMALY.md to rig it with Mixamo.")
    log("done - drag GauntAnomaly (C++ Classes > MyProject5) into a level, or rebuild the forest map.")


main()
