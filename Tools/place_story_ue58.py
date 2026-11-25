"""
place_story_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...) with
/Game/Maps/AH_ForestResearch_A07_Expanded open (after the build / setup / kit scripts).

The R.S.U. team that went in before you. Three dead operators (your Meshy Tactical Operator, posed by
Tools/pose_dead_operators_blender.py) and the blood that tells you what happened:

  1. STORAGE (first objective)  Operator slumped against the wall in a dried pool, spatter and drips on
     the wall behind him. He has the maintenance access card: the card objective is now on his body
     ("[F] Take maintenance access card").
  2. A-07 CHAMBER  A wet pool at the empty containment ring, then a smear where he crawled round it,
     face down at the end, one arm reaching for the magazine he never got to.
  3. THE WAY OUT  Bloody boot prints from the pool, round the console, out of the chamber, through
     maintenance and down the tunnel - the third operator, bleeding (drops all the way) - who died on his
     back a few metres short of the extraction beacon.

Imports (re-runs are fine): /Game/ForestResearch/Story - SM_AH_DeadOperator_Slumped/Prone/Supine +
M_AH_DeadOperator, blood textures, M_AH_BloodDecal (deferred decal) + MI_AH_Blood_* instances.
Bodies get low invisible collision (you step over the lying ones). Folder AnomalyHunter/A07_Story.
Not saved: look around, then Ctrl+S. Docs/A07_PLAYABLE.md
"""

import math
import os
import unreal

TARGET_WORLD = "AH_ForestResearch_A07_Expanded"
DST = "/Game/ForestResearch/Story"
FOLDER = unreal.Name("AnomalyHunter/A07_Story")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

LEVEL = {}


def log(msg):
    unreal.log("[Story] " + str(msg))


def warn(msg):
    unreal.log_warning("[Story] " + str(msg))


def try_set(obj, names, value, quiet=False):
    for n in ([names] if isinstance(names, str) else names):
        try:
            obj.set_editor_property(n, value)
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


ROOT = os.path.dirname(tools_dir())


def folder(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def import_file(path, dest, name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    if options:
        task.set_editor_property("options", options)
    asset_tools.import_asset_tasks([task])


def texture(path, dest, name, srgb, comp):
    if not os.path.isfile(path):
        warn("missing " + path)
        return None
    import_file(path, dest, name)
    t = eal.load_asset(dest + "/" + name)
    if t:
        t.set_editor_property("srgb", srgb)
        t.set_editor_property("compression_settings", comp)
        eal.save_loaded_asset(t)
    return t


def new_material(name, dest):
    path = dest + "/" + name
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if isinstance(mat, unreal.Material):
        mel.delete_all_material_expressions(mat)
        return mat
    return asset_tools.create_asset(name, dest, unreal.Material, unreal.MaterialFactoryNew())


# ----------------------------------------------------------------------------- the bodies
def import_bodies():
    src = os.path.join(ROOT, "Assets", "DeadOperators")
    tdst = DST + "/Textures"
    folder(tdst)
    tex = {
        "D": texture(os.path.join(src, "T_AH_DeadOperator_D.jpg"), tdst, "T_AH_DeadOperator_D", True,
                     unreal.TextureCompressionSettings.TC_DEFAULT),
        "N": texture(os.path.join(src, "T_AH_DeadOperator_N.png"), tdst, "T_AH_DeadOperator_N", False,
                     unreal.TextureCompressionSettings.TC_NORMALMAP),
        "MR": texture(os.path.join(src, "T_AH_DeadOperator_MR.jpg"), tdst, "T_AH_DeadOperator_MR", False,
                      unreal.TextureCompressionSettings.TC_MASKS),
    }
    mat = new_material("M_AH_DeadOperator", DST)
    if tex["D"]:
        d = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, -200)
        d.set_editor_property("texture", tex["D"])
        mel.connect_material_property(d, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if tex["N"]:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 100)
        n.set_editor_property("texture", tex["N"])
        try_set(n, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if tex["MR"]:
        mr = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -500, 400)
        mr.set_editor_property("texture", tex["MR"])
        try_set(mr, "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mel.connect_material_property(mr, "G", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(mr, "B", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)

    meshes = {}
    for pose in ("Slumped", "Prone", "Supine"):
        fbx = os.path.join(src, "SM_AH_DeadOperator_%s.fbx" % pose)
        if not os.path.isfile(fbx):
            warn("missing " + fbx)
            continue
        target = DST + "/SM_AH_DeadOperator_" + pose
        tmp = DST + "/_Import_" + pose
        folder(tmp)
        opts = unreal.FbxImportUI()
        try_set(opts, "import_as_skeletal", False)
        try_set(opts, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        try_set(opts, "import_materials", False)
        try_set(opts, "import_textures", False)
        sm = opts.get_editor_property("static_mesh_import_data")
        try_set(sm, "combine_meshes", True)
        try_set(sm, "auto_generate_collision", False)
        try_set(sm, "build_nanite", True, quiet=True)
        try_set(sm, "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        try_set(opts, "static_mesh_import_data", sm)
        import_file(fbx, tmp, "SM_AH_DeadOperator_" + pose, opts)
        mesh = None
        for p in eal.list_assets(tmp, recursive=True, include_folder=False):
            obj = eal.load_asset(p)
            if isinstance(obj, unreal.StaticMesh):
                if eal.does_asset_exist(target):
                    eal.delete_asset(target)
                eal.rename_asset(obj.get_path_name().split(".")[0], target)
                mesh = eal.load_asset(target)
                break
        for p in eal.list_assets(tmp, recursive=True, include_folder=False):
            eal.delete_asset(p.split(".")[0])
        eal.delete_directory(tmp)
        if mesh:
            for i in range(len(mesh.get_editor_property("static_materials"))):
                mesh.set_material(i, mat)
            eal.save_loaded_asset(mesh)
            meshes[pose] = mesh
    return meshes


# ----------------------------------------------------------------------------- blood decals
BLOOD = {   # instance: (texture, roughness, opacity, tint)
    "Pool": ("T_AH_Blood_Pool", 0.12, 1.0, (1, 1, 1)),
    "PoolDried": ("T_AH_Blood_Pool", 0.55, 0.95, (0.6, 0.5, 0.42)),
    "Smear": ("T_AH_Blood_Smear", 0.25, 1.0, (1, 1, 1)),
    "Spatter": ("T_AH_Blood_Spatter", 0.3, 1.0, (1, 1, 1)),
    "SpatterDried": ("T_AH_Blood_Spatter", 0.6, 0.9, (0.6, 0.5, 0.42)),
    "Boot": ("T_AH_Blood_Boot", 0.45, 0.95, (1, 1, 1)),
    "BootB": ("T_AH_Blood_BootB", 0.45, 0.95, (1, 1, 1)),
    "BootFaint": ("T_AH_Blood_Boot", 0.55, 0.5, (0.8, 0.7, 0.65)),
    "BootBFaint": ("T_AH_Blood_BootB", 0.55, 0.5, (0.8, 0.7, 0.65)),
    "Drops": ("T_AH_Blood_Drops", 0.2, 1.0, (1, 1, 1)),
}


def blood_materials():
    src = os.path.join(ROOT, "Assets", "Textures", "Blood")
    tdst = DST + "/Blood"
    folder(tdst)
    tex = {}
    for name in sorted({v[0] for v in BLOOD.values()}):
        tex[name] = texture(os.path.join(src, name + ".png"), tdst, name, True,
                            unreal.TextureCompressionSettings.TC_DEFAULT)

    mat = new_material("M_AH_BloodDecal", DST)
    try_set(mat, "material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    try_set(mat, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    s = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -800, 0)
    s.set_editor_property("parameter_name", "Decal")
    s.set_editor_property("texture", tex.get("T_AH_Blood_Pool"))
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -800, -250)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    base = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, -150)
    mel.connect_material_expressions(s, "RGB", base, "A")
    mel.connect_material_expressions(tint, "", base, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    op = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 300)
    op.set_editor_property("parameter_name", "Opacity")
    op.set_editor_property("default_value", 1.0)
    opm = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -450, 150)
    mel.connect_material_expressions(s, "A", opm, "A")
    mel.connect_material_expressions(op, "", opm, "B")
    mel.connect_material_property(opm, "", unreal.MaterialProperty.MP_OPACITY)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -450, 350)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.2)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)

    mis = {}
    for key, (tname, r, o, t) in BLOOD.items():
        name = "MI_AH_Blood_" + key
        path = DST + "/" + name
        mi = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if not isinstance(mi, unreal.MaterialInstanceConstant):
            mi = asset_tools.create_asset(name, DST, unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, mat)
        if tex.get(tname):
            mel.set_material_instance_texture_parameter_value(mi, "Decal", tex[tname])
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", r)
        mel.set_material_instance_scalar_parameter_value(mi, "Opacity", o)
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(t[0], t[1], t[2], 1))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        mis[key] = mi
    return mis


# ----------------------------------------------------------------------------- placing
def spawn(cls, label, pos, rot=None):
    if label in LEVEL:
        return None
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*pos), rot or unreal.Rotator(0, 0, 0))
    if not a:
        warn("couldn't spawn " + label)
        return None
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    LEVEL[label] = a
    return a


def decal(label, mi, pos, length, width, yaw=0.0, wall=False, depth=14.0, sort=0):
    """Floor decal (projects down) or wall decal (projects along yaw, image top = up).
    length runs along the image's width (U), width along its height (V)."""
    rot = unreal.Rotator(roll=-90.0, pitch=0.0, yaw=yaw) if wall else unreal.Rotator(roll=0.0, pitch=-90.0, yaw=yaw)
    a = spawn(unreal.DecalActor, label, pos, rot)
    if a is None:
        return
    comp = a.get_component_by_class(unreal.DecalComponent)
    comp.set_decal_material(mi)
    try_set(comp, "decal_size", unreal.Vector(depth, width / 2.0, length / 2.0))
    try_set(comp, "sort_order", sort, quiet=True)
    try_set(comp, "fade_screen_size", 0.0, quiet=True)      # don't vanish at a distance


def body(label, mesh, pos, yaw, blockers):
    a = spawn(unreal.StaticMeshActor, label, pos, unreal.Rotator(0, 0, yaw))
    if a is None:
        return
    a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_enable_collision(False)
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    cube = eal.load_asset("/Engine/BasicShapes/Cube")
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    for i, (lx, ly, lz, sx, sy, sz) in enumerate(blockers):     # body-local boxes (x forward/head)
        b = spawn(unreal.StaticMeshActor, "%s_Collision_%d" % (label, i),
                  (pos[0] + lx * c - ly * s, pos[1] + lx * s + ly * c, pos[2] + lz), unreal.Rotator(0, 0, yaw))
        if b is None:
            continue
        comp = b.static_mesh_component
        comp.set_static_mesh(cube)
        b.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0))
        comp.set_visibility(False)
        b.set_actor_hidden_in_game(True)
        # F (Visibility) goes through to whatever's on the body; you and the Gaunt don't
        comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY,
                                               unreal.CollisionResponseType.ECR_IGNORE)


def walk(points, stride):
    """Positions + headings every `stride` cm along a polyline."""
    out = []
    carry = 0.0
    for (x0, y0), (x1, y1) in zip(points[:-1], points[1:]):
        seg = math.hypot(x1 - x0, y1 - y0)
        heading = math.degrees(math.atan2(y1 - y0, x1 - x0))
        d = carry
        while d < seg:
            t = d / seg
            out.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, heading))
            d += stride
        carry = d - seg
    return out


def main():
    world = editor.get_editor_world()
    if not world or world.get_name() != TARGET_WORLD:
        unreal.log_error("[Story] open %s first (you have %s)" % (TARGET_WORLD, world.get_name() if world else "none"))
        return
    LEVEL.update({a.get_actor_label(): a for a in actors.get_all_level_actors() if a})
    folder(DST)
    meshes = import_bodies()
    mis = blood_materials()
    if len(meshes) < 3:
        warn("not all dead operator meshes imported - git pull (Assets/DeadOperators)")

    # ---- 1. storage: slumped against the partition wall, facing the door, the card on him
    if "Slumped" in meshes:
        # body-local boxes: torso against the wall (full height), legs (low enough to step over)
        body("A07S_DeadOperator_Storage", meshes["Slumped"], (1226, -600, 0), 180.0,
             [(-33, 0, 45, 50, 60, 90), (25, 0, 12, 70, 70, 24)])
    decal("A07S_Blood_StoragePool", mis["PoolDried"], (1200, -600, 5), 170, 170, 20.0, sort=0)
    decal("A07S_Blood_StorageWall", mis["SpatterDried"], (1286, -600, 135), 130, 130, 0.0, wall=True)
    card = LEVEL.get("A07_MaintenanceAccessCard")
    if card is not None:
        card.set_actor_location(unreal.Vector(1205, -600, 70), False, False)
        try_set(card, ["show_panel", "b_show_panel"], False)
        try_set(card, ["panel_offset"], unreal.Vector(0, 0, 0))
        try_set(card, ["label"], "Operator's access card")
    else:
        warn("A07_MaintenanceAccessCard not found - run setup_a07_playable_ue58.py; the card stays where it is")

    # ---- 2. chamber: killed at the empty ring, crawled round it, died reaching for the magazine
    decal("A07S_Blood_RingPool", mis["Pool"], (10170, 980, 5), 210, 210, 35.0)
    decal("A07S_Blood_RingSpatter", mis["Spatter"], (10110, 880, 5), 160, 160, 200.0, sort=1)
    crawl = [(10180, 1000), (10230, 1250), (10420, 1340), (10666, 1386)]
    for i, (x, y, h) in enumerate(walk(crawl, 130)):
        decal("A07S_Blood_Crawl_%02d" % i, mis["Smear"], (x, y, 5), 210, 75, h, sort=2)
    if "Prone" in meshes:
        body("A07S_DeadOperator_Chamber", meshes["Prone"], (10760, 1420, 0), 20.0,
             [(0, 0, 14, 180, 50, 28)])
    decal("A07S_Blood_ChamberBody", mis["Pool"], (10730, 1410, 5), 120, 110, 10.0, sort=1)

    # ---- 3. the way out: boot prints and drops from the ring to the tunnel, a body by the exit
    path_parts = [
        [(10250, 800), (10350, 300), (10450, -300), (10900, -700), (11120, -880)],   # chamber
        [(11280, -900), (13120, -900)],                                                # maintenance
        [(13280, -900), (14400, -1010)],                                               # tunnel
    ]
    n = 0
    for part_index, part in enumerate(path_parts):
        for i, (x, y, h) in enumerate(walk(part, 72.0)):
            left = (n % 2 == 0)
            side = 11.0 if left else -11.0
            ox, oy = -math.sin(math.radians(h)) * side, math.cos(math.radians(h)) * side
            faint = part_index > 0 or i > 10                   # fresh blood on the first few steps only
            key = ("Boot" if left else "BootB") + ("Faint" if faint else "")
            decal("A07S_Blood_Print_%03d" % n, mis[key], (x + ox, y + oy, 5), 31, 15.5,
                  h + (6.0 if left else -6.0), sort=3)
            if n % 3 == 1:
                decal("A07S_Blood_Drip_%03d" % n, mis["Drops"], (x - oy * 1.5, y + ox * 1.5, 5), 28, 28,
                      (n * 47) % 360, sort=3)
            n += 1
    if "Supine" in meshes:
        body("A07S_DeadOperator_Tunnel", meshes["Supine"], (14550, -1060, 0), 0.0,
             [(0, 0, 12, 170, 55, 24)])
    decal("A07S_Blood_TunnelPool", mis["Pool"], (14570, -1050, 5), 150, 140, 75.0, sort=1)
    decal("A07S_Blood_TunnelWall", mis["Spatter"], (14480, -1322, 90), 70, 70, -90.0, wall=True)

    # the ambience stings the first time you see each body (AHAmbienceComponent): tag them, old and new
    for label, a in LEVEL.items():
        if label.startswith("A07S_DeadOperator_") and "_Collision" not in label:
            tags = list(a.get_editor_property("tags"))
            if unreal.Name("AH_Stinger") not in tags:
                tags.append(unreal.Name("AH_Stinger"))
                a.set_editor_property("tags", tags)

    log("done: 3 operators, %d boot prints + the pools, crawl and spatter. The access card objective is on "
        "the operator in storage. Look around (torch on), then Ctrl+S." % n)


main()
