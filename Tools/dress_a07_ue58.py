"""
dress_a07_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...) with
/Game/Maps/AH_ForestResearch_A07_Expanded open, after build_a07_expanded_ue58.py and
setup_a07_playable_ue58.py.

Makes the A-07 wing look and sound like a real place, using YOUR imported assets:

  1. Asset scan: lists every Static Mesh you've imported (outside /Engine) and writes it to
     Saved/AH_AssetInventory.txt (send me that file and I can use the rest of your stuff).
     Looks for your Meshy vessel / decon chamber / control console wherever you imported them
     and puts them in the map if build_a07_expanded_ue58.py couldn't find them.
  2. Meshy props through the wing (SM_FR_* from Tools/import_meshy_forest_ue58.py and the A-07
     modules): PPE shelves and crates in the airlock, workstations in observation, crate cover and
     shelving in the chamber, two generators + racks in maintenance, ducts, floor grates, biohazard
     and exit signs, an RSU panel by the exit.
  3. Real collision for floor props: an invisible box per shelf / crate / desk / generator, so you
     (and bullets and the Gaunt) stop at them, and the NavMesh goes round them.
  4. Light fittings: a hanging industrial lamp over every A-07 light, with the light moved to just
     under it, plus new powered lights in the link corridor, spine, chamber and tunnel. They come on
     with backup power and die in the breach (AH_PoweredLight); they buzz while lit.
  5. Generators (A-07 maintenance + the original generator room) tagged AH_MachineHum: they start
     rumbling when you restore power.
  6. Room acoustics: Reverb Effect assets in /Game/Generated/Reverb and an Audio Volume per room:
     short and dead in offices, a ring in the steel airlock, a long concrete tail in the 9 m
     chamber, a flutter down the tunnel.

Safe to re-run (labels that exist are skipped). New actors go in AnomalyHunter/A07_Dressing.
NOT saved for you: look around, press P to check the NavMesh, then Ctrl+S.
Docs/A07_PLAYABLE.md
"""

import math
import os
import unreal

TARGET_WORLD = "AH_ForestResearch_A07_Expanded"
FOLDER = unreal.Name("AnomalyHunter/A07_Dressing")
PROPS = "/Game/ForestResearch/MeshyProps"
MODULES = "/Game/ForestResearch/MeshyExpansion/Modules"
REVERB_DIR = "/Game/Generated/Reverb"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[A07 Dressing] " + str(msg))


def warn(msg):
    unreal.log_warning("[A07 Dressing] " + str(msg))


# ----------------------------------------------------------------------------- what goes where
# Model fronts as imported: +Y, except the exit sign (-Y). Facing = which way the front ends up.
FRONT = {"SM_FR_ExitFixture": "-Y"}
DIRECTION_ANGLE = {"+X": 0.0, "+Y": 90.0, "-X": 180.0, "-Y": 270.0}

# Envelopes (cm): width along the front face, depth, height. Same real sizes as the original
# facility's placement pass, so everything matches.
SHELF = (180, 80, 245)
CRATE = (95, 70, 90)
DUCT = (260, 40, 90)
BIG_DUCT = (400, 60, 120)
DESK = (170, 145, 130)
GEN = (300, 150, 200)
GRATE = (57, 57, 10)
BIOHAZARD = (60, 4, 62)
EXIT = (45, 24, 35)
RSU = (180, 4, 194)

# Room ceilings (underside of the builder's roof slabs)
C_LINK, C_AIRLOCK, C_SPINE, C_OBS, C_CHAMBER, C_MAINT, C_TUNNEL = 330, 350, 330, 350, 900, 440, 340

P = []   # (mesh, label, (x, y, z), envelope, facing, anchor, ceiling, solid, tags)


def prop(mesh, label, pos, env, facing, anchor="floor", ceiling=0, solid=False, tags=()):
    P.append((mesh, label, pos, env, facing, anchor, ceiling, solid, tuple(tags)))


# --- link corridor (x 4000..4700, y +/-216): before the security door
prop("SM_FR_CeilingDuct", "A07D_Duct_Link", (4350, 150, 0), DUCT, "+Y", "ceiling", C_LINK)
prop("SM_FR_BiohazardSign", "A07D_Biohazard_Link", (4300, -214, 135), BIOHAZARD, "+Y")

# --- airlock (x 4714..5986, y +/-536)
for i, y in enumerate((-330, 330)):
    prop("SM_FR_BiohazardSign", "A07D_Biohazard_Airlock_%d" % i, (4716, y, 140), BIOHAZARD, "+X")
prop("SM_FR_ExitFixture", "A07D_Exit_Airlock", (4728, 0, 282), EXIT, "+X")
prop("SM_FR_IndustrialShelf", "A07D_PPEShelf_0", (4900, 492, 0), SHELF, "-Y", solid=True)
prop("SM_FR_IndustrialShelf", "A07D_PPEShelf_1", (5850, 492, 0), SHELF, "-Y", solid=True)
prop("SM_FR_StorageCrate", "A07D_Crate_Airlock", (5150, -480, 0), CRATE, "+Y", solid=True)
prop("SM_FR_RSUWallPanel", "A07D_RSUPanel_Airlock", (5700, -534, 0), RSU, "+Y")

# --- spine (x 6000..7400, y +/-216)
prop("SM_FR_CeilingDuct", "A07D_Duct_Spine_0", (6300, 150, 0), DUCT, "+Y", "ceiling", C_SPINE)
prop("SM_FR_CeilingDuct", "A07D_Duct_Spine_1", (7050, -150, 0), DUCT, "-Y", "ceiling", C_SPINE)
for x in (6150, 6425, 6700, 6975, 7250):
    prop("SM_FR_FloorGrate", "A07D_Grate_Spine_%d" % x, (x, -120, -6), GRATE, "+Y")
prop("SM_FR_BiohazardSign", "A07D_Biohazard_Spine", (7250, -214, 150), BIOHAZARD, "+Y")

# --- observation room (x 6264..7386, y 564..1836)
prop("SM_FR_ResearchWorkstation", "A07D_ObsDesk_0", (6340, 1450, 0), DESK, "+X", solid=True)
prop("SM_FR_ResearchWorkstation", "A07D_ObsDesk_1", (6340, 1050, 0), DESK, "+X", solid=True)
prop("SM_FR_IndustrialShelf", "A07D_ObsShelf", (7150, 1792, 0), SHELF, "-Y", solid=True)
prop("SM_FR_CeilingDuct", "A07D_Duct_Obs", (6800, 1780, 0), DUCT, "-Y", "ceiling", C_OBS)
prop("SM_FR_ExitFixture", "A07D_Exit_Obs", (6700, 576, 282), EXIT, "+Y")

# --- A-07 chamber (x 7414..11186, y +/-1886, 9 m high)
for i, (x, y, f) in enumerate(((8300, 1450, "+Y"), (8450, -950, "+X"), (10150, 1350, "+X"),
                               (10300, -1250, "+Y"), (8700, 600, "+X"))):
    prop("SM_FR_StorageCrate", "A07D_Crate_Chamber_%d" % i, (x, y, 0), CRATE, f, solid=True)
prop("SM_FR_IndustrialShelf", "A07D_ChamberShelf_0", (7900, 1842, 0), SHELF, "-Y", solid=True)
prop("SM_FR_IndustrialShelf", "A07D_ChamberShelf_1", (10600, 1842, 0), SHELF, "-Y", solid=True)
prop("SM_FR_CeilingDuct", "A07D_Duct_Chamber_0", (8500, -1820, 0), BIG_DUCT, "+Y", "ceiling", C_CHAMBER)
prop("SM_FR_CeilingDuct", "A07D_Duct_Chamber_1", (10000, 1820, 0), BIG_DUCT, "-Y", "ceiling", C_CHAMBER)
for i, y in enumerate((-450, 450)):
    prop("SM_FR_BiohazardSign", "A07D_Biohazard_Chamber_%d" % i, (7416, y, 170), BIOHAZARD, "+X")
prop("SM_FR_ExitFixture", "A07D_Exit_ChamberEast", (11174, -900, 285), EXIT, "-X")

# --- maintenance (x 11214..13186, y -1586..-264)
for i, x in enumerate((11650, 12450)):
    prop("SM_FR_Generator", "A07D_Generator_%d" % i, (x, -1505, 0), GEN, "+Y", solid=True, tags=("AH_MachineHum",))
prop("SM_FR_IndustrialShelf", "A07D_MaintShelf_0", (11550, -308, 0), SHELF, "-Y", solid=True)
prop("SM_FR_IndustrialShelf", "A07D_MaintShelf_1", (12950, -308, 0), SHELF, "-Y", solid=True)
prop("SM_FR_StorageCrate", "A07D_Crate_Maint", (13000, -1450, 0), CRATE, "+Y", solid=True)
prop("SM_FR_CeilingDuct", "A07D_Duct_Maint", (12200, -450, 0), DUCT, "-Y", "ceiling", C_MAINT)
prop("SM_FR_ExitFixture", "A07D_Exit_Maint", (13174, -900, 285), EXIT, "-X")

# --- utility tunnel (x 13214..15286, y -1336..-464)
for i, x in enumerate((13700, 14500)):
    prop("SM_FR_CeilingDuct", "A07D_Duct_Tunnel_%d" % i, (x, -1290, 0), DUCT, "+Y", "ceiling", C_TUNNEL)
for x in (13450, 13760, 14070, 14380, 14690):
    prop("SM_FR_FloorGrate", "A07D_Grate_Tunnel_%d" % x, (x, -900, -6), GRATE, "+Y")
prop("SM_FR_StorageCrate", "A07D_Crate_Tunnel", (14900, -560, 0), CRATE, "+Y", solid=True)
prop("SM_FR_ExitFixture", "A07D_Exit_Tunnel", (15274, -900, 285), EXIT, "-X")
prop("SM_FR_RSUWallPanel", "A07D_RSUPanel_Outside", (15316, -1250, 0), RSU, "+X")

# A-07 modules (front unknown: bounds-fitted like build_a07_expanded_ue58.py; the second damaged
# wall mirrors the builder's first one onto the north wall)
MODULE_PLACEMENTS = [
    ("DamagedWall/SM_A07_DamagedWall", "A07D_DamagedWall_North", (9900, 1790, 0), (440, 110, 520), 180.0),
]

# Light fittings: existing A-07 lights to hang a lamp over, and new powered lights.
# (label, x, y, ceiling, fixture envelope)
LAMP = (120, 30, 80)
BIG_LAMP = (220, 55, 150)
EXISTING_LAMPS = [
    ("A07_AirlockPowerLamp", 5350, 0, C_AIRLOCK, LAMP),
    ("A07_ObservationPowerLamp", 6700, 1130, C_OBS, LAMP),
    ("A07_ChamberPowerLamp1", 8300, 0, C_CHAMBER, BIG_LAMP),
    ("A07_ChamberPowerLamp2", 10300, 0, C_CHAMBER, BIG_LAMP),
    ("A07_MaintenancePowerLamp", 12200, -950, C_MAINT, LAMP),
]
NEW_LAMPS = [   # label, x, y, ceiling, fixture, intensity, radius
    ("A07D_LinkPowerLamp", 4300, 0, C_LINK, LAMP, 1100, 650),
    ("A07D_SpinePowerLamp", 6700, 0, C_SPINE, LAMP, 1100, 700),
    ("A07D_ChamberPowerLamp3", 9300, 1500, C_CHAMBER, BIG_LAMP, 1400, 900),
    ("A07D_ChamberPowerLamp4", 9300, -1500, C_CHAMBER, BIG_LAMP, 1400, 900),
    ("A07D_TunnelPowerLamp_0", 13800, -900, C_TUNNEL, LAMP, 1000, 650),
    ("A07D_TunnelPowerLamp_1", 14600, -900, C_TUNNEL, LAMP, 1000, 650),
]

# Generators in the original facility (Meshy ones, or the blockout ones if the Meshy pass wasn't run)
BASE_GENERATORS = ["AH_Meshy_Generator_0", "AH_Meshy_Generator_1", "FR_GeneratorFrame_0", "FR_GeneratorFrame_1"]

# Reverb: (asset name, settings). UE ReverbEffect ranges: decay 0.1-20 s, gains 0-1 (late up to 10).
REVERBS = {
    "RE_AH_Office":      dict(density=0.6, diffusion=0.8, gain=0.32, gain_hf=0.55, decay_time=0.55, decay_hf_ratio=0.7,
                              reflections_gain=0.25, reflections_delay=0.004, late_gain=1.0, late_delay=0.008),
    "RE_AH_Corridor":    dict(density=0.8, diffusion=0.55, gain=0.32, gain_hf=0.7, decay_time=1.25, decay_hf_ratio=0.8,
                              reflections_gain=0.4, reflections_delay=0.008, late_gain=1.3, late_delay=0.014),
    "RE_AH_SteelAirlock": dict(density=1.0, diffusion=0.7, gain=0.32, gain_hf=0.9, decay_time=1.0, decay_hf_ratio=1.25,
                              reflections_gain=0.6, reflections_delay=0.003, late_gain=1.4, late_delay=0.006),
    "RE_AH_Chamber":     dict(density=1.0, diffusion=1.0, gain=0.32, gain_hf=0.5, decay_time=4.6, decay_hf_ratio=0.7,
                              reflections_gain=0.15, reflections_delay=0.04, late_gain=1.7, late_delay=0.03),
    "RE_AH_Plantroom":   dict(density=1.0, diffusion=0.8, gain=0.32, gain_hf=0.6, decay_time=1.7, decay_hf_ratio=0.75,
                              reflections_gain=0.35, reflections_delay=0.012, late_gain=1.4, late_delay=0.018),
    "RE_AH_Tunnel":      dict(density=1.0, diffusion=0.45, gain=0.32, gain_hf=0.6, decay_time=2.4, decay_hf_ratio=0.8,
                              reflections_gain=0.55, reflections_delay=0.012, late_gain=1.8, late_delay=0.02),
}
# label, centre, half size, reverb, priority
ROOMS = [
    ("A07D_Reverb_FRCorridor", (2000, 0, 165), (2000, 200, 165), "RE_AH_Corridor", 1.0),
    ("A07D_Reverb_FRReception", (650, 852, 165), (650, 648, 165), "RE_AH_Office", 1.0),
    ("A07D_Reverb_FRLab", (2000, 852, 165), (700, 648, 165), "RE_AH_Office", 1.0),
    ("A07D_Reverb_FRServer", (3350, 852, 165), (650, 648, 165), "RE_AH_Office", 1.0),
    ("A07D_Reverb_FRStorage", (650, -852, 165), (650, 648, 165), "RE_AH_Office", 1.0),
    ("A07D_Reverb_FRMedical", (2000, -852, 165), (700, 648, 165), "RE_AH_Office", 1.0),
    ("A07D_Reverb_FRGenerator", (3350, -852, 165), (650, 648, 165), "RE_AH_Plantroom", 1.0),
    ("A07D_Reverb_Link", (4350, 0, 165), (350, 216, 165), "RE_AH_Corridor", 2.0),
    ("A07D_Reverb_Airlock", (5350, 0, 175), (636, 536, 175), "RE_AH_SteelAirlock", 2.0),
    ("A07D_Reverb_Spine", (6700, 0, 165), (700, 216, 165), "RE_AH_Corridor", 2.0),
    ("A07D_Reverb_ControlAccess", (6700, 390, 175), (200, 160, 175), "RE_AH_Corridor", 2.0),
    ("A07D_Reverb_Observation", (6825, 1200, 175), (561, 636, 175), "RE_AH_Office", 2.0),
    ("A07D_Reverb_Chamber", (9300, 0, 450), (1886, 1886, 450), "RE_AH_Chamber", 1.0),
    ("A07D_Reverb_Maintenance", (12200, -925, 220), (986, 661, 220), "RE_AH_Plantroom", 2.0),
    ("A07D_Reverb_Tunnel", (14250, -900, 170), (1036, 436, 170), "RE_AH_Tunnel", 2.0),
]

# Your Meshy hero pieces: where the builder expects them, and words to look for if they're elsewhere
HERO = [
    ("A07_MeshyContainmentVessel", "/Game/ForestResearch/MeshyExpansion/ContainmentVessel/SM_A07_ContainmentVessel",
     [["vessel"], ["containment", "tank"], ["containment", "pod"], ["containment", "cell"]],
     (9300, 0, 130), (610, 610, 590), 0.0),
    ("A07_MeshyDeconChamber", "/Game/ForestResearch/MeshyExpansion/DeconChamber/SM_A07_DeconChamber",
     [["decon"], ["decontamination"]], (5450, 370, 0), (540, 320, 280), 90.0),
    ("A07_MeshyControlConsole", "/Game/ForestResearch/MeshyExpansion/ControlConsole/SM_A07_ControlConsole",
     [["controlconsole"], ["control", "console"], ["console"]], (6840, 1320, 0), (380, 200, 230), 0.0),
]


# ----------------------------------------------------------------------------- helpers
def by_label():
    return {a.get_actor_label(): a for a in actors.get_all_level_actors() if a}


def set_prop(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            continue
    warn("couldn't set %s on %s" % (names[0], obj.get_name()))
    return False


def add_tags(actor, tags):
    current = list(actor.get_editor_property("tags"))
    changed = False
    for t in tags:
        if unreal.Name(t) not in current:
            current.append(unreal.Name(t))
            changed = True
    if changed:
        actor.set_editor_property("tags", current)


def label_actor(a, label):
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)


def load_mesh(path):
    if not lib.does_asset_exist(path):
        return None
    asset = lib.load_asset(path)
    return asset if isinstance(asset, unreal.StaticMesh) else None


_REGISTRY = None


def all_meshes():
    """(name, object path, package folder) of every Static Mesh under /Game."""
    global _REGISTRY
    if _REGISTRY is None:
        _REGISTRY = []
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        try:
            found = reg.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Engine", "StaticMesh"), False)
        except Exception:
            found = reg.get_assets_by_class("StaticMesh", False)
        for data in found:
            folder = str(data.package_path)
            if not folder.startswith("/Game"):
                continue
            name = str(data.asset_name)
            _REGISTRY.append((name, "%s.%s" % (str(data.package_name), name), folder))
        _REGISTRY.sort(key=lambda r: (r[2], r[0]))
    return _REGISTRY


def write_inventory(world):
    meshes = all_meshes()
    folders = {}
    for name, path, folder in meshes:
        folders.setdefault(folder, []).append(name)
    lines = ["Static Meshes under /Game in %s's project (%d)" % (world.get_name(), len(meshes)), ""]
    for folder in sorted(folders):
        names = folders[folder]
        lines.append("%s  (%d)" % (folder, len(names)))
        for n in names[:60]:
            lines.append("    " + n)
        if len(names) > 60:
            lines.append("    ... %d more" % (len(names) - 60))
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                       "AH_AssetInventory.txt")
    try:
        with open(out, "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        log("asset inventory (%d meshes in %d folders) written to %s" % (len(meshes), len(folders), out))
    except Exception as exc:
        warn("couldn't write the inventory: %s" % exc)
    for folder in sorted(folders):
        if any(k in folder.lower() for k in ("meshy", "forestresearch", "a07", "anomaly", "facility")):
            log("  %s: %s" % (folder, ", ".join(folders[folder][:12])))


def search_mesh(word_sets):
    """First /Game Static Mesh whose name contains every word of one of the sets (generated stuff skipped)."""
    for words in word_sets:
        for name, path, folder in all_meshes():
            low = name.lower().replace("_", "")
            if folder.startswith("/Game/Generated") or folder.startswith("/Game/Maps"):
                continue
            if all(w in low for w in words):
                asset = lib.load_asset(path)
                if isinstance(asset, unreal.StaticMesh):
                    return asset, path
    return None, None


def mesh_bounds(mesh):
    box = mesh.get_bounding_box()
    lo, hi = box.min, box.max
    spans = (float(hi.x - lo.x), float(hi.y - lo.y), float(hi.z - lo.z))
    if min(spans) <= 0.00001:
        raise ValueError("flat or empty bounds")
    return spans, ((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5), float(lo.z)


def place_mesh(mesh, label, pos, env, yaw, anchor="floor", ceiling=0.0):
    """Uniformly scale mesh to fit env (in its own frame), turn it by yaw, put its bottom-centre at pos
    (or hang it from the ceiling). Visual only: no collision on dense Meshy geometry."""
    spans, mids, min_z = mesh_bounds(mesh)
    factor = min(env[0] / spans[0], env[1] / spans[1], env[2] / spans[2])
    if not (0.0001 <= factor <= 1000.0):
        raise ValueError("unsafe scale %.5f" % factor)
    r = math.radians(yaw)
    rx = mids[0] * math.cos(r) - mids[1] * math.sin(r)
    ry = mids[0] * math.sin(r) + mids[1] * math.cos(r)
    bottom = ceiling - spans[2] * factor if anchor == "ceiling" else pos[2]
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                      unreal.Vector(pos[0] - rx * factor, pos[1] - ry * factor, bottom - min_z * factor),
                                      unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    if not a:
        raise RuntimeError("couldn't spawn")
    label_actor(a, label)
    comp = a.static_mesh_component
    comp.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(factor, factor, factor))
    a.set_actor_enable_collision(False)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    try:
        comp.set_editor_property("can_ever_affect_navigation", False)
    except Exception:
        pass
    return a, spans[2] * factor


def yaw_for(mesh_name, facing):
    yaw = DIRECTION_ANGLE[facing] - DIRECTION_ANGLE[FRONT.get(mesh_name, "+Y")]
    return (yaw + 180.0) % 360.0 - 180.0


def add_blocker(owner, cube):
    """Invisible box round a placed prop: collision + NavMesh obstacle, nothing to render."""
    label = owner.get_actor_label() + "_Collision"
    origin, extent = owner.get_actor_bounds(False)
    b = actors.spawn_actor_from_class(unreal.StaticMeshActor, origin)
    if not b:
        return
    label_actor(b, label)
    b.static_mesh_component.set_static_mesh(cube)
    b.set_actor_scale3d(unreal.Vector(max(extent.x, 2) * 0.019, max(extent.y, 2) * 0.019, max(extent.z, 2) * 0.02))
    b.static_mesh_component.set_visibility(False)
    b.set_actor_hidden_in_game(True)


def make_reverb(name, settings):
    path = REVERB_DIR + "/" + name
    if lib.does_asset_exist(path):
        asset = lib.load_asset(path)
    else:
        factory = None
        if hasattr(unreal, "ReverbEffectFactory"):
            try:
                factory = unreal.ReverbEffectFactory()
            except Exception:
                factory = None
        asset = asset_tools.create_asset(name, REVERB_DIR, unreal.ReverbEffect, factory)
    if not asset:
        warn("couldn't create reverb %s" % name)
        return None
    for k, v in settings.items():
        try:
            asset.set_editor_property(k, float(v))
        except Exception as exc:
            warn("%s.%s: %s" % (name, k, exc))
    lib.save_loaded_asset(asset)
    return asset


# ----------------------------------------------------------------------------- main
def main():
    world = editor.get_editor_world()
    if not world or world.get_name() != TARGET_WORLD:
        unreal.log_error("[A07 Dressing] open %s first (you have %s)" % (TARGET_WORLD, world.get_name() if world else "none"))
        return
    cube = lib.load_asset("/Engine/BasicShapes/Cube")
    level = by_label()
    if "A07_ChamberFloor" not in level:
        unreal.log_error("[A07 Dressing] this map has no A-07 wing - run build_a07_expanded_ue58.py first")
        return

    # 1. what have you imported?
    write_inventory(world)
    for label, path, words, pos, env, yaw in HERO:
        if label in level:
            continue
        mesh = load_mesh(path)
        found_at = path
        if mesh is None:
            mesh, found_at = search_mesh(words)
        if mesh is None:
            log("no %s found (looked for %s) - fine, it's optional" % (label, " / ".join("+".join(w) for w in words)))
            continue
        try:
            place_mesh(mesh, label, pos, env, yaw)
            log("placed your %s as %s" % (found_at, label))
            if label == "A07_MeshyContainmentVessel" and "A07_CentralSimpleCollision" in level:
                level["A07_CentralSimpleCollision"].static_mesh_component.set_visibility(False)  # the vessel covers it
        except Exception as exc:
            warn("%s: %s" % (label, exc))

    # 2 + 3. props
    meshes, placed, missing = {}, 0, set()
    for mesh_name, label, pos, env, facing, anchor, ceiling, solid, tags in P:
        if label in level:
            continue
        if mesh_name not in meshes:
            meshes[mesh_name] = load_mesh(PROPS + "/" + mesh_name)
            if meshes[mesh_name] is None:
                found, path = search_mesh([[mesh_name.lower().replace("_", "")]])
                meshes[mesh_name] = found
                if found:
                    log("using %s for %s" % (path, mesh_name))
        mesh = meshes[mesh_name]
        if mesh is None:
            missing.add(mesh_name)
            continue
        try:
            a, _ = place_mesh(mesh, label, pos, env, yaw_for(mesh_name, facing), anchor, ceiling)
            if tags:
                add_tags(a, tags)
            if solid and cube:
                add_blocker(a, cube)
            placed += 1
        except Exception as exc:
            warn("%s: %s" % (label, exc))
    if missing:
        warn("not imported, skipped: %s (run Tools/import_meshy_forest_ue58.py)" % ", ".join(sorted(missing)))

    for rel, label, pos, env, yaw in MODULE_PLACEMENTS:
        if label in level:
            continue
        mesh = load_mesh(MODULES + "/" + rel)
        if mesh is None:
            continue
        try:
            place_mesh(mesh, label, pos, env, yaw)
            placed += 1
        except Exception as exc:
            warn("%s: %s" % (label, exc))

    # 4. light fittings + new powered lights
    lamp_mesh = meshes.get("SM_FR_HangingLight") or load_mesh(PROPS + "/SM_FR_HangingLight")
    lamps = [(lab, x, y, c, fx, None, None) for lab, x, y, c, fx in EXISTING_LAMPS] + list(NEW_LAMPS)
    level = by_label()
    for label, x, y, ceiling, fixture, intensity, radius in lamps:
        drop = fixture[2]
        fix_label = label + "_Fixture"
        if fix_label in level:
            origin, extent = level[fix_label].get_actor_bounds(False)
            drop = ceiling - (origin.z - extent.z)         # already hung: keep the light under it
        elif lamp_mesh is not None:
            try:
                # the big chamber lamps run along Y, between the roof girders (which run along Y every 5.5 m)
                facing = "+X" if fixture is BIG_LAMP else "+Y"
                fx_actor, drop = place_mesh(lamp_mesh, fix_label, (x, y, 0), fixture, yaw_for("SM_FR_HangingLight", facing),
                                            "ceiling", ceiling)
                fx_actor.static_mesh_component.set_editor_property("cast_shadow", False)   # don't trap its own light
            except Exception as exc:
                warn("%s: %s" % (fix_label, exc))
        light = level.get(label)
        if light is None and intensity is not None:
            light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, ceiling - drop - 12))
            if light:
                label_actor(light, label)
                comp = light.get_component_by_class(unreal.PointLightComponent)
                comp.set_intensity(float(intensity))
                comp.set_attenuation_radius(float(radius))
                comp.set_light_color(unreal.LinearColor(0.75, 0.87, 1.0, 1.0))
                comp.set_editor_property("cast_shadows", True)
                comp.set_visibility(False)          # off until backup power is restored
                add_tags(light, ["AH_PoweredLight"])
        if light is not None:
            light.get_editor_property("root_component").set_mobility(unreal.ComponentMobility.MOVABLE)
            light.set_actor_location(unreal.Vector(x, y, ceiling - drop - 12), False, False)

    # 5. generators rumble once power is back
    hums = 0
    for label in BASE_GENERATORS + ["A07D_Generator_0", "A07D_Generator_1"]:
        a = level.get(label)
        if a is not None:
            add_tags(a, ["AH_MachineHum"])
            hums += 1

    # 6. room acoustics
    reverbs = {name: make_reverb(name, s) for name, s in REVERBS.items()}
    rooms = 0
    for label, centre, half, reverb, priority in ROOMS:
        effect = reverbs.get(reverb)
        if effect is None:
            continue
        vol = level.get(label)
        if vol is None:
            vol = actors.spawn_actor_from_class(unreal.AudioVolume, unreal.Vector(*centre))
            if vol is None:
                continue
            label_actor(vol, label)
        vol.set_actor_location(unreal.Vector(*centre), False, False)
        vol.set_actor_scale3d(unreal.Vector(half[0] / 100.0, half[1] / 100.0, half[2] / 100.0))   # 200 cm brush
        rs = unreal.ReverbSettings()
        set_prop(rs, ["apply_reverb", "b_apply_reverb"], True)
        set_prop(rs, ["reverb_effect"], effect)
        set_prop(rs, ["volume"], 0.55)
        set_prop(rs, ["fade_time"], 0.6)
        set_prop(vol, ["settings"], rs)
        set_prop(vol, ["priority"], float(priority))
        rooms += 1
    check = by_label().get("A07D_Reverb_Chamber")
    if check is not None and check.get_actor_bounds(False)[1].x < 1.0:
        warn("Audio Volumes came out with no size (no brush): place them by hand, see Docs/A07_PLAYABLE.md")

    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    log("done: %d props/modules placed, %d lamps fitted, %d generators humming, %d reverb rooms. "
        "Press P to check the NavMesh goes round the new props, then Ctrl+S." % (placed, len(lamps), hums, rooms))


main()
