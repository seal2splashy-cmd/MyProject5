"""
ue_setup_project.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...).

Does three things, and is safe to re-run any time (it replaces what it made before):
  1. Imports Assets/Audio/*.wav and Assets/Textures/*.png into /Game/Generated/...
  2. Builds materials from them (world-aligned ones for level geometry, UV ones for weapons)
  3. Builds /Game/Maps/TestArena: a dark concrete warehouse with corridors, physics crates,
     barrels, flickery-looking lights, a bodycam post-process, the player start and enemies.

Requires the Python Editor Script Plugin + Editor Scripting Utilities (enabled in the .uproject).
"""

import os
import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ASSET_SRC = os.path.join(PROJECT_DIR, "Assets")

AUDIO_DST = "/Game/Generated/Audio"
TEX_DST = "/Game/Generated/Textures"
MAT_DST = "/Game/Generated/Materials"
MAP_PATH = "/Game/Maps/TestArena"
TAG = "GeneratedArena"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log("[MyProject5 setup] " + msg)


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as e:  # property names drift between engine versions
        unreal.log_warning("[MyProject5 setup] couldn't set %s: %s" % (prop, e))
        return False


# ======================================================================== 1. import

def import_folder(src_dir, dst_path, exts):
    if not os.path.isdir(src_dir):
        unreal.log_warning("[MyProject5 setup] missing folder " + src_dir)
        return []
    tasks = []
    for name in sorted(os.listdir(src_dir)):
        if os.path.splitext(name)[1].lower() not in exts:
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(src_dir, name))
        t.set_editor_property("destination_path", dst_path)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    asset_tools.import_asset_tasks(tasks)
    imported = []
    for t in tasks:
        imported += list(t.get_editor_property("imported_object_paths"))
    log("imported %d files into %s" % (len(imported), dst_path))
    return imported


def fix_audio():
    loop = AUDIO_DST + "/SW_Ambience_BodycamHiss_Loop"
    if eal.does_asset_exist(loop):
        wave = eal.load_asset(loop)
        try_set(wave, "looping", True)
        eal.save_loaded_asset(wave)


def fix_textures():
    for name in os.listdir(os.path.join(ASSET_SRC, "Textures")):
        base = os.path.splitext(name)[0]
        path = TEX_DST + "/" + base
        if not eal.does_asset_exist(path):
            continue
        tex = eal.load_asset(path)
        if base.endswith("_N"):
            try_set(tex, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            try_set(tex, "srgb", False)
        elif base.endswith("_R"):
            try_set(tex, "compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            try_set(tex, "srgb", False)
        eal.save_loaded_asset(tex)


# ======================================================================== 2. materials

def tex(name):
    path = TEX_DST + "/" + name
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


def new_material(name):
    path = MAT_DST + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return asset_tools.create_asset(name, MAT_DST, unreal.Material, unreal.MaterialFactoryNew())


def world_uv(mat, tile_cm, x, y):
    """Planar world UVs that work on axis-aligned floors AND walls with one texture sample:
    floors use (X, Y), walls use (X+Y, Z) - one of X/Y is constant along any axis-aligned wall."""
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, x - 900, y)

    mask_xy = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x - 700, y - 80)
    mask_xy.set_editor_property("r", True); mask_xy.set_editor_property("g", True)
    mel.connect_material_expressions(wp, "", mask_xy, "")

    mask_x = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x - 700, y + 20)
    mask_x.set_editor_property("r", True)
    mask_y = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x - 700, y + 100)
    mask_y.set_editor_property("g", True)
    mask_z = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x - 700, y + 180)
    mask_z.set_editor_property("b", True)
    for m in (mask_x, mask_y, mask_z):
        mel.connect_material_expressions(wp, "", m, "")

    add_xy = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, x - 520, y + 60)
    mel.connect_material_expressions(mask_x, "", add_xy, "A")
    mel.connect_material_expressions(mask_y, "", add_xy, "B")

    wall_uv = mel.create_material_expression(mat, unreal.MaterialExpressionAppendVector, x - 380, y + 100)
    mel.connect_material_expressions(add_xy, "", wall_uv, "A")
    mel.connect_material_expressions(mask_z, "", wall_uv, "B")

    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, x - 700, y + 280)
    nz = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x - 560, y + 280)
    nz.set_editor_property("b", True)
    mel.connect_material_expressions(nrm, "", nz, "")
    nz_abs = mel.create_material_expression(mat, unreal.MaterialExpressionAbs, x - 440, y + 280)
    mel.connect_material_expressions(nz, "", nz_abs, "")
    nz_round = mel.create_material_expression(mat, unreal.MaterialExpressionRound, x - 340, y + 280)
    mel.connect_material_expressions(nz_abs, "", nz_round, "")

    pick = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, x - 240, y + 60)
    mel.connect_material_expressions(wall_uv, "", pick, "A")
    mel.connect_material_expressions(mask_xy, "", pick, "B")
    mel.connect_material_expressions(nz_round, "", pick, "Alpha")

    scale = mel.create_material_expression(mat, unreal.MaterialExpressionDivide, x - 120, y + 60)
    try_set(scale, "const_b", float(tile_cm))
    mel.connect_material_expressions(pick, "", scale, "A")
    return scale


def mesh_uv(mat, tiling, x, y):
    tc = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, x - 200, y)
    try_set(tc, "u_tiling", float(tiling))
    try_set(tc, "v_tiling", float(tiling))
    return tc


def build_material(name, diffuse, normal=None, rough_tex=None, roughness=0.8, metallic=0.0,
                   world_tile_cm=None, uv_tiling=1.0):
    d = tex(diffuse)
    if d is None:
        unreal.log_warning("[MyProject5 setup] no texture %s, skipping %s" % (diffuse, name))
        return None
    mat = new_material(name)
    uv = world_uv(mat, world_tile_cm, 0, 0) if world_tile_cm else mesh_uv(mat, uv_tiling, 0, 0)

    s_d = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, 0, -200)
    s_d.set_editor_property("texture", d)
    mel.connect_material_expressions(uv, "", s_d, "UVs")
    mel.connect_material_property(s_d, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    n = tex(normal) if normal else None
    if n:
        s_n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, 0, 100)
        s_n.set_editor_property("texture", n)
        try_set(s_n, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(uv, "", s_n, "UVs")
        mel.connect_material_property(s_n, "RGB", unreal.MaterialProperty.MP_NORMAL)

    r = tex(rough_tex) if rough_tex else None
    if r:
        s_r = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, 0, 400)
        s_r.set_editor_property("texture", r)
        try_set(s_r, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mel.connect_material_expressions(uv, "", s_r, "UVs")
        mel.connect_material_property(s_r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        c_r = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 200, 400)
        c_r.set_editor_property("r", roughness)
        mel.connect_material_property(c_r, "", unreal.MaterialProperty.MP_ROUGHNESS)

    if metallic > 0:
        c_m = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, 200, 500)
        c_m.set_editor_property("r", metallic)
        mel.connect_material_property(c_m, "", unreal.MaterialProperty.MP_METALLIC)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    log("material " + name)
    return mat


def build_materials():
    mats = {}
    # level geometry: world-aligned, so scaled cubes don't stretch the texture
    mats["concrete"] = build_material("M_Concrete_World", "T_Concrete_D", "T_Concrete_N", roughness=0.9, world_tile_cm=300)
    mats["floor"] = build_material("M_FloorTiles_World", "T_FloorTiles_D", "T_FloorTiles_N", roughness=0.75, world_tile_cm=240)
    mats["rust_world"] = build_material("M_RustMetal_World", "T_RustMetal_D", "T_RustMetal_N", "T_RustMetal_R", metallic=0.6, world_tile_cm=150)
    mats["wood_world"] = build_material("M_Wood_World", "T_Wood_D", roughness=0.85, world_tile_cm=120)
    # weapons / small props: mesh UVs (names match the paths in MeleeWeaponTypes.cpp)
    mats["rust"] = build_material("M_RustMetal", "T_RustMetal_D", "T_RustMetal_N", "T_RustMetal_R", metallic=0.6, uv_tiling=2.0)
    mats["wood"] = build_material("M_Wood", "T_Wood_D", roughness=0.85, uv_tiling=1.0)
    return mats


# ======================================================================== 3. level

CUBE = None
CYL = None


def actor_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(cls, loc, rot=(0, 0, 0)):
    """rot is (pitch, yaw, roll). unreal.Rotator's positional order is (roll, pitch, yaw),
    so always pass keywords."""
    rotator = unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2])
    a = actor_sub().spawn_actor_from_class(cls, unreal.Vector(*loc), rotator)
    if a:
        tags = list(a.get_editor_property("tags"))
        tags.append(TAG)
        a.set_editor_property("tags", tags)
    return a


def block(loc, size, mat, label=None, physics=False, cylinder=False, rot=(0, 0, 0)):
    """size in cm (the engine cube/cylinder are 100 cm)."""
    a = spawn(unreal.StaticMeshActor, loc, rot)
    smc = a.static_mesh_component
    if physics:
        smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    smc.set_static_mesh(CYL if cylinder else CUBE)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if mat:
        smc.set_material(0, mat)
    if physics:
        smc.set_simulate_physics(True)
    if label:
        a.set_actor_label(label)
    return a


def light(loc, intensity, colour, radius, label):
    a = spawn(unreal.PointLight, loc)
    comp = a.get_component_by_class(unreal.PointLightComponent)
    try_set(comp, "intensity_units", unreal.LightUnits.CANDELAS)
    comp.set_intensity(intensity)
    comp.set_light_color(unreal.LinearColor(*colour))
    comp.set_attenuation_radius(radius)
    try_set(comp, "cast_shadows", True)
    a.set_actor_label(label)
    return a


def bodycam_post_process():
    ppv = spawn(unreal.PostProcessVolume, (0, 0, 200))
    ppv.set_actor_label("BodycamPostProcess")
    try_set(ppv, "unbound", True)
    s = ppv.get_editor_property("settings")
    wanted = {
        # lock exposure so the dark stays dark
        "override_auto_exposure_min_brightness": True, "auto_exposure_min_brightness": 1.0,
        "override_auto_exposure_max_brightness": True, "auto_exposure_max_brightness": 1.0,
        # cheap body-cam lens
        "override_vignette_intensity": True, "vignette_intensity": 0.9,
        "override_film_grain_intensity": True, "film_grain_intensity": 0.35,
        "override_scene_fringe_intensity": True, "scene_fringe_intensity": 2.5,
        "override_bloom_intensity": True, "bloom_intensity": 1.2,
        "override_motion_blur_amount": True, "motion_blur_amount": 0.6,
        "override_color_saturation": True, "color_saturation": unreal.Vector4(0.8, 0.8, 0.8, 1.0),
    }
    for k, v in wanted.items():
        try_set(s, k, v)
    try_set(ppv, "settings", s)


def clear_previous():
    for a in actor_sub().get_all_level_actors():
        if TAG in [str(t) for t in a.get_editor_property("tags")]:
            actor_sub().destroy_actor(a)


def build_level(mats):
    global CUBE, CYL
    CUBE = eal.load_asset("/Engine/BasicShapes/Cube")
    CYL = eal.load_asset("/Engine/BasicShapes/Cylinder")

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if eal.does_asset_exist(MAP_PATH):
        les.load_level(MAP_PATH)
        clear_previous()
    else:
        les.new_level(MAP_PATH)

    concrete = mats.get("concrete")
    floor = mats.get("floor")
    rust = mats.get("rust_world")
    wood = mats.get("wood_world")

    H = 420.0     # wall height
    T = 40.0      # wall thickness
    S = 4000.0    # arena size

    # shell
    block((0, 0, -10), (S, S, 20), floor, "Floor")
    block((0, 0, H + 10), (S, S, 20), concrete, "Ceiling")
    for (x, y, sx, sy, n) in [(0, S / 2, S, T, "N"), (0, -S / 2, S, T, "S"),
                              (S / 2, 0, T, S, "E"), (-S / 2, 0, T, S, "W")]:
        block((x, y, H / 2), (sx, sy, H), concrete, "Wall_" + n)

    # inner walls: a corridor ring with gaps, plus a central room with two doorways
    inner = [
        # (centre x, centre y, length x, length y)
        (-1000, 1000, 1600, T), (1100, 1000, 1400, T),          # north corridor wall, gap at x~0
        (-1100, -1000, 1400, T), (1000, -1000, 1600, T),        # south corridor wall, gap at x~-100
        (-1000, 0, T, 1200), (1000, 300, T, 600), (1000, -650, T, 300),   # east/west room walls
        (-500, 1550, T, 900), (600, -1550, T, 900),             # dead ends in the outer ring
    ]
    for i, (x, y, sx, sy) in enumerate(inner):
        block((x, y, H / 2), (sx, sy, H), concrete, "InnerWall_%d" % i)

    # pillars
    for i, (x, y) in enumerate([(-1500, 1500), (1500, 1500), (-1500, -1500), (1500, -1500), (0, 0)]):
        block((x, y, H / 2), (60, 60, H), concrete, "Pillar_%d" % i)

    # physics crates to smash with the melee, some stacked
    crates = [(-300, 300, 0), (-300, 300, 1), (-180, 320, 0), (400, -300, 0), (1500, 400, 0),
              (1450, 520, 0), (1500, 460, 1), (-1600, -300, 0), (-1600, -180, 0), (200, 1500, 0)]
    for i, (x, y, level) in enumerate(crates):
        block((x, y, 50 + level * 102), (100, 100, 100), wood, "Crate_%d" % i, physics=True,
              rot=(0, (i * 37) % 30 - 15, 0))

    # rusty barrels
    for i, (x, y) in enumerate([(-700, -300), (-640, -380), (700, 1300), (-1300, 1300), (1300, -1300), (260, -700)]):
        block((x, y, 50), (60, 60, 100), rust, "Barrel_%d" % i, physics=True, cylinder=True)

    # lighting: almost nothing, so the flashlight-less bodycam really is scary
    sky = spawn(unreal.SkyLight, (0, 0, 600))
    try_set(sky.get_component_by_class(unreal.SkyLightComponent), "intensity", 0.08)
    sky.set_actor_label("FaintAmbient")
    # candelas, with exposure locked at EV100 1 by the post-process: pools of light, black between.
    # If it's too dark/bright on your monitor, nudge auto_exposure_min/max_brightness in the PPV.
    lights = [((0, 0, 380), 40, (1.0, 0.85, 0.6), 900, "Light_Centre"),
              ((-1500, 0, 380), 18, (0.6, 1.0, 0.7), 700, "Light_West_Sickly"),
              ((1500, -1500, 380), 25, (1.0, 0.5, 0.35), 800, "Light_SE_Red"),
              ((0, 1500, 380), 14, (0.7, 0.8, 1.0), 700, "Light_North_Cold"),
              ((-1500, -1500, 380), 10, (1.0, 0.9, 0.7), 600, "Light_SW_Dim")]
    for loc, inten, col, rad, lbl in lights:
        light(loc, inten, col, rad, lbl)

    fog = spawn(unreal.ExponentialHeightFog, (0, 0, 0))
    try_set(fog.get_component_by_class(unreal.ExponentialHeightFogComponent), "fog_density", 0.05)
    fog.set_actor_label("Fog")

    bodycam_post_process()

    # player start in the south-west corridor, facing the middle
    ps = spawn(unreal.PlayerStart, (-1700, -1700, 100), (0, 45, 0))
    ps.set_actor_label("PlayerStart")

    # enemies
    smart = unreal.load_class(None, "/Script/MyProject5.SmartEnemy")
    basic = unreal.load_class(None, "/Script/MyProject5.Enemy")
    for i, (cls, loc) in enumerate([(smart, (1250, 1700, 100)), (smart, (300, 300, 100)),
                                    (basic, (-1250, 1700, 100)), (basic, (1500, -600, 100))]):
        if cls:
            e = spawn(cls, loc, (0, 180, 0))
            e.set_actor_label("Enemy_%d_%s" % (i, "Smart" if cls == smart else "Basic"))
        else:
            unreal.log_warning("[MyProject5 setup] enemy class not found - build the C++ first")

    les.save_current_level()
    log("built " + MAP_PATH)


# ======================================================================== run

def main():
    with unreal.ScopedSlowTask(5, "MyProject5: importing assets and building test arena") as task:
        task.make_dialog(True)
        task.enter_progress_frame(1, "Importing audio")
        import_folder(os.path.join(ASSET_SRC, "Audio"), AUDIO_DST, {".wav"})
        fix_audio()
        task.enter_progress_frame(1, "Importing textures")
        import_folder(os.path.join(ASSET_SRC, "Textures"), TEX_DST, {".png"})
        fix_textures()
        task.enter_progress_frame(1, "Building materials")
        mats = build_materials()
        task.enter_progress_frame(1, "Building TestArena")
        build_level(mats)
        task.enter_progress_frame(1, "Done")
    log("All done - press Play in /Game/Maps/TestArena")


main()
