"""
retexture_a07_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...) with the level
to retexture open (made for AH_ForestResearch_A07_Expanded; works on any of the forest maps).

The blockout walls were one flat colour stretched over each box. This gives them the rugged,
stained cast concrete of the damaged wall panel, at real size on every wall:

  1. imports Assets/Textures/Rugged (Tools/generate_rugged_textures.py) to /Game/ForestResearch/Rugged
  2. builds M_AH_WorldAligned: box-projected from world position, so the texture tiles at its true size
     whatever the box's scale (walls use the face they're on, floors/ceilings the top), with
       - an anti-tiling layer (the same texture 7.5x bigger, varying the tone),
       - grime and damp darkening the bottom 70 cm of walls,
       - Tint / TileSize / NormalStrength / RoughnessScale / Grime parameters
  3. instances: MI_AH_RuggedWall (4 m tiles), MI_AH_WornFloor (3 m), MI_AH_StainedCeiling (dark)
  4. swaps them onto the open level: M_FR_Concrete -> wall, M_FR_Floor -> floor,
     M_FR_Charcoal on roofs/ceilings -> ceiling (other charcoal parts - cabinets, trays - are left alone)
  5. dusty air: an Exponential Height Fog with volumetric fog, so your torch throws a visible beam and
     the lamps glow in the haze (the torch already has volumetric scattering set up)

Footsteps / bullet impacts still read these as concrete. Safe to re-run. Not saved: look, then Ctrl+S.
To undo on a level: Ctrl+Z, or set the materials back (they're only swapped, nothing is deleted).
"""

import os
import unreal

DST = "/Game/ForestResearch/Rugged"
FOG_LABEL = "AH_DustFog"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log("[Retexture] " + str(msg))


def warn(msg):
    unreal.log_warning("[Retexture] " + str(msg))


def try_set(obj, names, value, quiet=False):
    for name in ([names] if isinstance(names, str) else names):
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            continue
    if not quiet:
        warn("couldn't set %s on %s" % (names, obj.get_name()))
    return False


def tools_dir():
    try:
        return os.path.dirname(os.path.abspath(__file__))
    except NameError:
        return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")


SRC = os.path.join(os.path.dirname(tools_dir()), "Assets", "Textures", "Rugged")


# ----------------------------------------------------------------------------- textures
def import_texture(name, ext, srgb, comp):
    dst = DST + "/Textures"
    if not eal.does_directory_exist(dst):
        eal.make_directory(dst)
    path = os.path.join(SRC, name + ext)
    if not os.path.isfile(path):
        warn("missing " + path + " (git pull, or python Tools/generate_rugged_textures.py)")
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dst)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    t = eal.load_asset(dst + "/" + name)
    if t:
        t.set_editor_property("srgb", srgb)
        t.set_editor_property("compression_settings", comp)
        eal.save_loaded_asset(t)
    return t


def texture_set(base):
    return {
        "D": import_texture(base + "_D", ".jpg", True, unreal.TextureCompressionSettings.TC_DEFAULT),
        "N": import_texture(base + "_N", ".png", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
        "MR": import_texture(base + "_MR", ".jpg", False, unreal.TextureCompressionSettings.TC_MASKS),
    }


# ----------------------------------------------------------------------------- the master material
def build_master(tex):
    name = "M_AH_WorldAligned"
    path = DST + "/" + name
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(name, DST, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        e = mel.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(a, out, b, inp):
        if not mel.connect_material_expressions(a, out, b, inp):
            warn("couldn't connect %s.%s -> %s.%s" % (a.get_name(), out, b.get_name(), inp))

    def scalar(pname, value, x, y):
        return node(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=pname, default_value=value)

    def mask(src, out, ch, x, y):
        m = node(unreal.MaterialExpressionComponentMask, x, y, r=(ch == "r"), g=(ch == "g"), b=(ch == "b"), a=False)
        link(src, out, m, "")
        return m

    def binop(cls, a, a_out, b, b_out, x, y, **props):
        e = node(cls, x, y, **props)
        link(a, a_out, e, "A")
        if b is not None:
            link(b, b_out, e, "B")
        return e

    def lerp(a, b, alpha, x, y, a_out="", b_out="", alpha_out="", **props):
        e = node(unreal.MaterialExpressionLinearInterpolate, x, y, **props)
        if a is not None:
            link(a, a_out, e, "A")
        if b is not None:
            link(b, b_out, e, "B")
        link(alpha, alpha_out, e, "Alpha")
        return e

    # world position / tile size -> 3 planar projections, picked by the face normal
    wp = node(unreal.MaterialExpressionWorldPosition, -2400, 0)
    tile = scalar("TileSize", 400.0, -2400, 150)
    p = binop(unreal.MaterialExpressionDivide, wp, "", tile, "", -2200, 0)
    px, py, pz = mask(p, "", "r", -2000, -100), mask(p, "", "g", -2000, 0), mask(p, "", "b", -2000, 100)
    npz = binop(unreal.MaterialExpressionMultiply, pz, "", None, "", -1850, 100, const_b=-1.0)   # texture V runs down
    uv_x = binop(unreal.MaterialExpressionAppendVector, py, "", npz, "", -1650, -150)       # faces looking along X
    uv_y = binop(unreal.MaterialExpressionAppendVector, px, "", npz, "", -1650, 0)          # faces looking along Y
    uv_z = binop(unreal.MaterialExpressionAppendVector, px, "", py, "", -1650, 150)         # floors / ceilings
    nrm = node(unreal.MaterialExpressionVertexNormalWS, -2400, 400)
    an = node(unreal.MaterialExpressionAbs, -2200, 400)
    link(nrm, "", an, "")
    ax = node(unreal.MaterialExpressionRound, -1850, 350)
    link(mask(an, "", "r", -2000, 350), "", ax, "")
    az = node(unreal.MaterialExpressionRound, -1850, 450)
    link(mask(an, "", "b", -2000, 450), "", az, "")
    uv1 = lerp(uv_y, uv_x, ax, -1450, 0)
    uv = lerp(uv1, uv_z, az, -1250, 0)

    def sampler(pname, texture, stype, x, y, uvs):
        s = node(unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=pname, texture=texture)
        try_set(s, "sampler_type", stype)
        link(uvs, "", s, "UVs")
        return s

    d = sampler("BaseColor", tex["D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1000, -300, uv)
    n = sampler("Normal", tex["N"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1000, 100, uv)
    mr = sampler("MR", tex["MR"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1000, 450, uv)

    # anti-tiling: the same texture 7.5x bigger, normalised to ~1, nudges the tone across a wall
    macro_uv = binop(unreal.MaterialExpressionMultiply, uv, "", scalar("MacroScale", 0.133, -1250, -500), "", -1100, -500)
    dm = sampler("BaseColor", tex["D"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -900, -650, macro_uv)
    macro = binop(unreal.MaterialExpressionMultiply, dm, "R", scalar("MacroGain", 4.0, -900, -450), "", -700, -600)
    one = node(unreal.MaterialExpressionConstant, -700, -700, r=1.0)
    macro_mix = lerp(one, macro, scalar("MacroAmount", 0.45, -700, -450), -550, -600)
    col = binop(unreal.MaterialExpressionMultiply, d, "RGB", macro_mix, "", -400, -350)

    # grime: walls darken towards the floor (not floors / ceilings themselves)
    wz = mask(wp, "", "b", -1000, 800)
    gz = binop(unreal.MaterialExpressionDivide, wz, "", scalar("GrimeHeight", 70.0, -1000, 900), "", -800, 800)
    gsat = node(unreal.MaterialExpressionSaturate, -650, 800)
    link(gz, "", gsat, "")
    galpha = binop(unreal.MaterialExpressionMax, gsat, "", az, "", -500, 800)
    dark = binop(unreal.MaterialExpressionMultiply, col, "", scalar("GrimeDarken", 0.55, -500, 650), "", -350, 650)
    grimed = lerp(dark, col, galpha, -250, -100)
    tint = node(unreal.MaterialExpressionVectorParameter, -250, -300, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    base = binop(unreal.MaterialExpressionMultiply, grimed, "", tint, "", -100, -200)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = binop(unreal.MaterialExpressionMultiply, mr, "G", scalar("RoughnessScale", 1.0, -700, 550), "", -400, 450)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(mr, "B", unreal.MaterialProperty.MP_METALLIC)

    flat = node(unreal.MaterialExpressionConstant3Vector, -700, 200, constant=unreal.LinearColor(0, 0, 1, 1))
    nmix = lerp(flat, n, scalar("NormalStrength", 1.0, -700, 300), -400, 150, b_out="RGB")
    mel.connect_material_property(nmix, "", unreal.MaterialProperty.MP_NORMAL)

    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def make_instance(name, parent, tex, scalars, tint=None):
    path = DST + "/" + name
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not isinstance(mi, unreal.MaterialInstanceConstant):
        mi = asset_tools.create_asset(name, DST, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for pname, key in (("BaseColor", "D"), ("Normal", "N"), ("MR", "MR")):
        if tex.get(key):
            mel.set_material_instance_texture_parameter_value(mi, pname, tex[key])
    for pname, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, pname, float(value))
    if tint:
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    return mi


# ----------------------------------------------------------------------------- level
def swap_materials(wall, floor, ceiling):
    counts = {"wall": 0, "floor": 0, "ceiling": 0}
    for a in actors.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        comp = a.static_mesh_component
        mat = comp.get_material(0)
        if mat is None:
            continue
        mname = mat.get_name()
        label = a.get_actor_label().lower()
        if mname == "M_FR_Concrete":
            comp.set_material(0, wall)
            counts["wall"] += 1
        elif mname == "M_FR_Floor":
            comp.set_material(0, floor)
            counts["floor"] += 1
        elif mname == "M_FR_Charcoal" and ("roof" in label or "ceiling" in label):
            comp.set_material(0, ceiling)
            counts["ceiling"] += 1
    return counts


def dust_fog():
    fog = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.ExponentialHeightFog)), None)
    if fog is None:
        fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
        fog.set_actor_label(FOG_LABEL)
        fog.set_folder_path(unreal.Name("AnomalyHunter/Atmosphere"))
    c = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    try_set(c, "fog_density", 0.012)
    try_set(c, "fog_height_falloff", 0.15)
    try_set(c, ["fog_inscattering_luminance", "fog_inscattering_color"], unreal.LinearColor(0.02, 0.022, 0.025, 1), quiet=True)
    try_set(c, ["volumetric_fog", "enable_volumetric_fog"], True)
    try_set(c, "volumetric_fog_scattering_distribution", 0.55, quiet=True)     # forward scatter: beams
    try_set(c, "volumetric_fog_albedo", unreal.Color(r=220, g=220, b=215, a=255), quiet=True)
    try_set(c, "volumetric_fog_extinction_scale", 1.4, quiet=True)
    try_set(c, "volumetric_fog_distance", 4000.0, quiet=True)
    return fog


def main():
    world = editor.get_editor_world()
    if world is None:
        unreal.log_error("[Retexture] no level open")
        return
    if not eal.does_directory_exist(DST):
        eal.make_directory(DST)
    wall_tex = texture_set("T_AH_RuggedConcrete")
    floor_tex = texture_set("T_AH_WornFloor")
    if not wall_tex["D"]:
        unreal.log_error("[Retexture] the rugged textures aren't in Assets/Textures/Rugged - git pull")
        return
    master = build_master(wall_tex)
    wall = make_instance("MI_AH_RuggedWall", master, wall_tex,
                         {"TileSize": 400, "MacroGain": 4.0, "GrimeDarken": 0.55, "NormalStrength": 1.0})
    floor = make_instance("MI_AH_WornFloor", master, floor_tex,
                          {"TileSize": 300, "MacroGain": 3.7, "MacroAmount": 0.35, "NormalStrength": 0.8})
    ceiling = make_instance("MI_AH_StainedCeiling", master, wall_tex,
                            {"TileSize": 500, "MacroGain": 4.0, "NormalStrength": 0.7}, tint=(0.42, 0.42, 0.44, 1))
    counts = swap_materials(wall, floor, ceiling)
    dust_fog()
    log("%s: %d walls, %d floors, %d ceilings retextured; dust fog on (torch beams). Look around, then Ctrl+S."
        % (world.get_name(), counts["wall"], counts["floor"], counts["ceiling"]))
    if world.get_name() != "AH_ForestResearch_A07_Expanded":
        warn("this isn't the A-07 map - fine if you meant it (Ctrl+Z to undo the swap)")


main()
