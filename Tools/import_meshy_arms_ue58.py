"""
import_meshy_arms_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Imports the Meshy operator arms built by Tools/build_meshy_arms_blender.py:
  Assets/OperatorArmsMeshy/SK_AH_OperatorArms_Meshy.fbx + T_AH_MeshyArms_D/N/MR
    -> /Game/Generated/Operator/SK_AH_OperatorArms_Meshy
It's the same rig as the original operator arms (upper arms kept, Meshy gloved forearms and
hands skinned on), so UOperatorArmsComponent picks it up on the next Play - no code changes,
and it goes back to the original arms if you delete this asset or untick bPreferMeshyArms.

Materials: the Meshy part gets M_AH_MeshyArms (colour, DirectX normal, roughness = G and
metallic = B of the MR map); the kept upper-arm slots reuse the operator's M_Op_* materials
(built here first via ue_setup_project.py if they don't exist yet).

Safe to re-run: re-imports and rebuilds in place.
"""

import os
import runpy
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT_DIR, "Assets", "OperatorArmsMeshy")
NAME = "SK_AH_OperatorArms_Meshy"           # UOperatorArmsComponent::MeshyArmsMesh
DST = "/Game/Generated/Operator"
TEX_DST = DST + "/Textures"
MAT_DST = DST + "/Materials"

# keyword in the FBX slot name -> operator material (same table as ue_setup_project.OP_MATERIALS)
OP_SLOTS = (("fabric", "M_Op_ContainmentFabric"), ("glove", "M_Op_GlovePolymer"),
            ("composite", "M_Op_ForearmGuard"), ("insignia", "M_Op_Insignia"),
            ("stitch", "M_Op_Stitching"), ("seam", "M_Op_SeamRecess"))

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[Meshy arms] " + msg)


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


def textures():
    folder(TEX_DST)
    out = {}
    for suffix, srgb, comp in (("D", True, unreal.TextureCompressionSettings.TC_DEFAULT),
                               ("N", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                               ("MR", False, unreal.TextureCompressionSettings.TC_MASKS)):
        tname = "T_AH_MeshyArms_" + suffix
        path = os.path.join(SRC, tname + ".png")
        if not os.path.isfile(path):
            continue
        import_file(path, TEX_DST, tname)
        t = eal.load_asset(TEX_DST + "/" + tname)
        if t:
            t.set_editor_property("srgb", srgb)
            t.set_editor_property("compression_settings", comp)
            eal.save_loaded_asset(t)
            out[suffix] = t
    return out


def meshy_material(tex):
    folder(MAT_DST)
    mname = "M_AH_MeshyArms"
    path = MAT_DST + "/" + mname
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)      # rebuild in place, keeps references
    else:
        if mat is not None:
            eal.delete_asset(path)
        mat = asset_tools.create_asset(mname, MAT_DST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)

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


def operator_materials():
    """The M_Op_* materials of the original arms; builds them (with the original arms) if missing."""
    if not all(eal.does_asset_exist(MAT_DST + "/" + m) for _, m in OP_SLOTS):
        log("operator materials missing - importing the original operator arms first")
        setup = runpy.run_path(os.path.join(PROJECT_DIR, "Tools", "ue_setup_project.py"),
                               init_globals={"MYPROJECT5_SETUP_AS_LIBRARY": True},
                               run_name="ue_setup_project_lib")
        setup["import_operator_arms"]()
    return {k: eal.load_asset(MAT_DST + "/" + m) for k, m in OP_SLOTS if eal.does_asset_exist(MAT_DST + "/" + m)}


def skeletal_mesh(fbx):
    target = DST + "/" + NAME
    tmp = DST + "/_Import_MeshyArms"      # own folder, so the original arms never get touched
    folder(tmp)
    opts = unreal.FbxImportUI()
    try_set(opts, "import_as_skeletal", True)
    try_set(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    try_set(opts, "import_mesh", True)
    try_set(opts, "import_materials", False)
    try_set(opts, "import_textures", False)
    try_set(opts, "import_animations", False)
    try_set(opts, "create_physics_asset", False)
    sk = opts.get_editor_property("skeletal_mesh_import_data")
    try_set(sk, "import_morph_targets", False)
    try_set(sk, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    try_set(opts, "skeletal_mesh_import_data", sk)
    import_file(fbx, tmp, NAME, opts)

    imported = [eal.load_asset(p) for p in eal.list_assets(tmp, recursive=True, include_folder=False)]
    mesh = next((a for a in imported if isinstance(a, unreal.SkeletalMesh)), None)
    if mesh is not None:
        # the mesh keeps the skeleton it was imported with; move both out of the temp folder
        skel = next((a for a in imported if isinstance(a, unreal.Skeleton)), None)
        if skel is not None:
            skel_target = DST + "/" + NAME + "_Skeleton"
            src = skel.get_path_name().split(".")[0]
            if src != skel_target:
                if eal.does_asset_exist(skel_target):
                    eal.delete_asset(skel_target)
                eal.rename_asset(src, skel_target)
        src = mesh.get_path_name().split(".")[0]
        if src != target:
            if eal.does_asset_exist(target):
                eal.delete_asset(target)
            eal.rename_asset(src, target)
        mesh = eal.load_asset(target)
    for p in eal.list_assets(tmp, recursive=True, include_folder=False):
        eal.delete_asset(p.split(".")[0])         # whatever else the importer made
    eal.delete_directory(tmp)
    return mesh


def main():
    fbx = os.path.join(SRC, NAME + ".fbx")
    if not os.path.isfile(fbx):
        log("no %s - run Tools/build_meshy_arms_blender.py first, skipped" % fbx)
        return
    folder(DST)
    op = operator_materials()
    meshy = meshy_material(textures())
    mesh = skeletal_mesh(fbx)
    if mesh is None:
        unreal.log_error("[Meshy arms] FBX imported but no skeletal mesh turned up")
        return

    slots = list(mesh.get_editor_property("materials"))
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name")).lower()
        if "meshy" in name:
            slot.set_editor_property("material_interface", meshy)
            continue
        for key, _ in OP_SLOTS:
            if key in name and op.get(key):
                slot.set_editor_property("material_interface", op[key])
                break
        else:
            unreal.log_warning("[Meshy arms] slot '%s' left as imported" % name)
    mesh.set_editor_property("materials", slots)
    eal.save_loaded_asset(mesh)
    log("imported %s (%d material slots) - press Play, the operator arms use it automatically" % (
        mesh.get_path_name(), len(slots)))


main()
