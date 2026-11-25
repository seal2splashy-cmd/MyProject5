"""
build_a07_kit_level_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...) with
/Game/Maps/AH_ForestResearch_A07_Expanded open, after Tools/import_a07_kit_ue58.py (and the
build / setup / dress scripts).

Builds the A-07 wing out of YOUR Meshy kit (/Game/ForestResearch/A07Kit):

  Catwalks   two walkable runs along the chamber's north and south walls, deck 3 m up on steel posts,
             each with a steel stair (treads, stringers, yellow handrails) down into the chamber.
             Invisible deck / rail / ramp collision in steel, so you clang along them, can't fall off,
             and the NavMesh (and the Gaunt) can follow you up. A magazine and a battery wait at the ends.
  Walls      your damaged-concrete panels cladding the chamber's north, south and east walls and the
             utility tunnel, each with its own collision.
  Cells      two sealed "B-7" vault doors in the chamber's east wall (other containment cells).
  Vessel     the containment tube standing in its mounting ring in the middle of the chamber, plus an
             empty ring by the containment console (where something used to be held).
  Consoles   the lab console in the observation room (screens to the room, back to the window) and by
             the containment ring, with the mission devices moved onto them (F still works).
  Decon      the decon pod along the airlock's north wall.

Your raw modules that the earlier scripts placed in awkward spots (the closed vault door standing in
the chamber's open doorway, the floating catwalks, the oversized decon/console, the second damaged
wall where the north catwalk now runs) are hidden, not deleted.

Safe to re-run (labels that exist are skipped). Everything goes in AnomalyHunter/A07_Kit.
NOT saved for you: walk it, press P (the NavMesh should cover both catwalk decks and stairs), Ctrl+S.
Needs the C++ from the same commit for the stairs: the player now walks up slopes and steps.
"""

import math
import unreal

TARGET_WORLD = "AH_ForestResearch_A07_Expanded"
FOLDER = unreal.Name("AnomalyHunter/A07_Kit")
KIT = "/Game/ForestResearch/A07Kit"
MATS = "/Game/ForestResearch/Materials"

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.EditorAssetLibrary

# Catwalk numbers measured from SM_A07K_Catwalk by prepare_a07_kit_blender.py (kit_report.json)
CW_LEN = 600.0          # one section
CW_DECK_Z = 76.0        # deck surface above the model's bottom
CW_DECK_W = 104.0
CW_RAIL_H = 109.0       # rail top above the deck
DECK_H = 300.0          # deck height above the chamber floor
STAIR_RUN = 520.0       # horizontal length of a stair (300 up over 520 along = 30 degrees)

FACING_YAW = {"+Y": 0.0, "-X": 90.0, "-Y": 180.0, "+X": -90.0}   # kit fronts are +Y

# Raw modules from the earlier scripts to hide (kit versions replace them)
HIDE = ["A07_MeshyCatwalkNorth", "A07_MeshyCatwalkSouth", "A07_MeshyContainmentEntrance",
        "A07_MeshyDeconChamber", "A07_MeshyControlConsole", "A07_MeshyContainmentVessel",
        "A07D_DamagedWall_North"]


def log(msg):
    unreal.log("[A07 Kit level] " + str(msg))


def warn(msg):
    unreal.log_warning("[A07 Kit level] " + str(msg))


def by_label():
    return {a.get_actor_label(): a for a in actors.get_all_level_actors() if a}


LEVEL = {}
CUBE = None
CYL = None
M = {}


def set_prop(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            continue
    warn("couldn't set %s on %s" % (names[0], obj.get_name()))
    return False


def new_actor(cls, label, loc, rot=None):
    if label in LEVEL:
        return None
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), rot or unreal.Rotator(0, 0, 0))
    if not a:
        warn("couldn't spawn " + label)
        return None
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    LEVEL[label] = a
    return a


def kit_mesh(name):
    path = "%s/SM_A07K_%s" % (KIT, name)
    if not lib.does_asset_exist(path):
        return None
    m = lib.load_asset(path)
    return m if isinstance(m, unreal.StaticMesh) else None


def place_kit(name, label, pos, facing="+Y", scale=1.0, yaw=None):
    """Kit meshes are real size with the pivot at the bottom centre: pos is where the bottom centre goes."""
    mesh = kit_mesh(name)
    if mesh is None or label in LEVEL:
        return LEVEL.get(label)
    r = unreal.Rotator(pitch=0, yaw=FACING_YAW[facing] if yaw is None else yaw, roll=0)
    a = new_actor(unreal.StaticMeshActor, label, pos, r)
    if a is None:
        return None
    a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.set_actor_enable_collision(False)
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    a.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
    return a


def box(label, centre, size, material=None, visible=False, rot=None, block_visibility=True, shape=None):
    """A box (or cylinder) from the engine shapes. Invisible ones are collision; visible ones are steelwork."""
    a = new_actor(unreal.StaticMeshActor, label, centre, rot)
    if a is None:
        return None
    comp = a.static_mesh_component
    comp.set_static_mesh(shape or CUBE)
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if material:
        comp.set_material(0, material)
    if not visible:
        comp.set_visibility(False)
        a.set_actor_hidden_in_game(True)
        if not block_visibility:
            # let the F trace through to a mission device inside it (bullets too, it's only a console)
            comp.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY,
                                                   unreal.CollisionResponseType.ECR_IGNORE)
    else:
        a.set_actor_enable_collision(False)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("can_ever_affect_navigation", False)
    return a


def blocker_for(actor, material, block_visibility=True, shrink=0.96):
    if actor is None:
        return
    origin, extent = actor.get_actor_bounds(False)
    box(actor.get_actor_label() + "_Collision", (origin.x, origin.y, origin.z),
        (extent.x * 2 * shrink, extent.y * 2 * shrink, extent.z * 2), material,
        block_visibility=block_visibility)


def sloped(label, a, b, width, thickness, material, visible, sink=0.0):
    """A box whose TOP surface runs from point a up to point b (a ramp, a stringer, a handrail)."""
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    run = math.hypot(dx, dy)
    length = math.hypot(run, dz)
    pitch = math.degrees(math.atan2(dz, run))
    yaw = math.degrees(math.atan2(dy, dx))
    p, y = math.radians(pitch), math.radians(yaw)
    up = (-math.sin(p) * math.cos(y), -math.sin(p) * math.sin(y), math.cos(p))
    half = thickness / 2.0 + sink
    centre = ((a[0] + b[0]) / 2 - up[0] * half, (a[1] + b[1]) / 2 - up[1] * half, (a[2] + b[2]) / 2 - up[2] * half)
    return box(label, centre, (length, width, thickness), material, visible,
               unreal.Rotator(pitch=pitch, yaw=yaw, roll=0))


# ----------------------------------------------------------------------------- catwalks
def catwalk_run(name, start_x, sections, y, stair_side):
    """Deck from start_x along +X for `sections`, stair coming up from the west (-X) into start_x."""
    steel, yellow = M.get("steel"), M.get("yellow")
    end_x = start_x + sections * CW_LEN
    base_z = DECK_H - CW_DECK_Z
    for i in range(sections):
        cx = start_x + (i + 0.5) * CW_LEN
        place_kit("Catwalk", "A07K_%s_Section_%d" % (name, i), (cx, y, base_z))
        for k, dx in enumerate((-260.0, 0.0, 260.0)):          # steel posts down to the floor under the legs
            for side in (-1, 1):
                box("A07K_%s_Post_%d_%d_%d" % (name, i, k, side + 1), (cx + dx, y + side * 40.0, base_z / 2.0),
                    (12, 12, base_z), steel, visible=True, shape=CYL)
    # walkable deck + rails (collision), steel so footsteps clang
    deck_len = end_x - start_x
    box("A07K_%s_Deck_Steel" % name, ((start_x + end_x) / 2, y, DECK_H - 6), (deck_len, CW_DECK_W, 12), steel)
    for side in (-1, 1):
        box("A07K_%s_Rail_%d_Steel" % (name, side + 1),
            ((start_x + end_x) / 2, y + side * (CW_DECK_W / 2 + 3), DECK_H + CW_RAIL_H / 2), (deck_len, 6, CW_RAIL_H), steel)
    box("A07K_%s_RailEnd_Steel" % name, (end_x + 3, y, DECK_H + CW_RAIL_H / 2), (6, CW_DECK_W + 12, CW_RAIL_H), steel)

    # the stair: from the floor at start_x - STAIR_RUN up to the deck at start_x
    x0, x1 = start_x - STAIR_RUN, start_x
    steps = 17
    rise, tread = DECK_H / steps, STAIR_RUN / steps
    for s in range(steps):
        box("A07K_%s_Tread_%02d" % (name, s), (x0 + (s + 0.5) * tread, y, (s + 1) * rise - 2), (tread + 4, CW_DECK_W - 10, 4),
            steel, visible=True)
    for side in (-1, 1):
        sy = y + side * (CW_DECK_W / 2 - 3)
        sloped("A07K_%s_Stringer_%d" % (name, side + 1), (x0 - 10, sy, 0), (x1, sy, DECK_H), 6, 22, steel, True, sink=-8)
        sloped("A07K_%s_Handrail_%d" % (name, side + 1), (x0, sy, 100), (x1, sy, DECK_H + 100), 5, 5, yellow, True)
        for k in range(4):                                      # handrail posts
            px = x0 + 40 + k * (STAIR_RUN - 80) / 3.0
            pz = (px - x0) / STAIR_RUN * DECK_H
            box("A07K_%s_HandrailPost_%d_%d" % (name, side + 1, k), (px, sy, pz + 50), (5, 5, 100), yellow, visible=True)
        # invisible stair rails so you can't step off the side
        sloped("A07K_%s_StairRail_%d_Steel" % (name, side + 1), (x0, y + side * (CW_DECK_W / 2 + 3), CW_RAIL_H),
               (x1, y + side * (CW_DECK_W / 2 + 3), DECK_H + CW_RAIL_H), 6, CW_RAIL_H, steel, False)
    # the walkable part: one smooth 30-degree ramp under the treads (player + NavMesh + the Gaunt)
    sloped("A07K_%s_StairRamp_Steel" % name, (x0 - 30, y, -17), (x1 + 2, y, DECK_H), CW_DECK_W - 4, 10, steel, False)


# ----------------------------------------------------------------------------- main
def main():
    global CUBE, CYL, LEVEL
    world = editor.get_editor_world()
    if not world or world.get_name() != TARGET_WORLD:
        unreal.log_error("[A07 Kit level] open %s first (you have %s)" % (TARGET_WORLD, world.get_name() if world else "none"))
        return
    if kit_mesh("Catwalk") is None:
        unreal.log_error("[A07 Kit level] the kit isn't imported - run Tools/import_a07_kit_ue58.py first")
        return
    LEVEL = by_label()
    if "A07_ChamberFloor" not in LEVEL:
        unreal.log_error("[A07 Kit level] no A-07 wing in this map - run build_a07_expanded_ue58.py first")
        return
    CUBE = lib.load_asset("/Engine/BasicShapes/Cube")
    CYL = lib.load_asset("/Engine/BasicShapes/Cylinder")
    for key, name in (("steel", "M_FR_Steel"), ("yellow", "M_FR_SafetyYellow"), ("concrete", "M_FR_Concrete")):
        path = MATS + "/" + name
        M[key] = lib.load_asset(path) if lib.does_asset_exist(path) else None
    steel, concrete = M.get("steel"), M.get("concrete")

    # hide the raw modules the kit replaces
    for label in HIDE:
        a = LEVEL.get(label)
        if a is not None:
            a.set_actor_hidden_in_game(True)
            a.set_is_temporarily_hidden_in_editor(True)
            a.set_actor_enable_collision(False)
    door = LEVEL.get("A07_Door_ChamberWest")
    if door is not None:
        set_prop(door, ["show_frame", "b_show_frame"], True)    # the vault door model isn't standing in it any more

    # catwalks (north: 4 sections, south: 3 - the south damaged wall is at the far end)
    catwalk_run("CatwalkN", 8160.0, 4, 1690.0, -1)
    catwalk_run("CatwalkS", 8160.0, 3, -1690.0, 1)
    pickup_cls = unreal.load_class(None, "/Script/MyProject5.AHPickup")
    enum = getattr(unreal, "AHPickupType", None)
    if pickup_cls and enum:
        for label, kind, pos in (("A07K_Pickup_Mag_CatwalkN", "MAGAZINE", (10450, 1690, DECK_H)),
                                 ("A07K_Pickup_Battery_CatwalkS", "BATTERY", (9850, -1690, DECK_H))):
            p = new_actor(pickup_cls, label, pos, unreal.Rotator(0, 0, 30))
            if p:
                set_prop(p, ["type"], getattr(enum, kind))

    # vessel in its ring (the builder's ring if it's there, otherwise the kit's)
    if "A07_MeshyMountingPlatform" not in LEVEL or LEVEL["A07_MeshyMountingPlatform"].is_temporarily_hidden_in_editor():
        place_kit("MountRing", "A07K_VesselRing", (9300, 0, 0))
    place_kit("Vessel", "A07K_Vessel", (9300, 0, 50), "-X")
    centre = LEVEL.get("A07_CentralSimpleCollision")
    if centre is not None:
        centre.static_mesh_component.set_visibility(False)       # the vessel is the visible thing now
    # an empty ring by the console: whatever it held isn't there any more
    ring = place_kit("MountRing", "A07K_EmptyRing", (10450, 1000, 0), yaw=37.0)
    if ring is not None:
        box("A07K_EmptyRing_Collision_Steel", (10450, 1000, 62), (490, 490, 124), steel, shape=CYL)

    # sealed cells: vault doors in the east wall, facing into the chamber
    for label, y in (("A07K_VaultDoor_B7", 600.0), ("A07K_VaultDoor_B8", 1290.0)):
        blocker_for(place_kit("VaultDoor", label, (11186 - 29.3, y, 0), "-X"), steel)

    # consoles, with the mission devices moved onto them
    obs = place_kit("Console", "A07K_ObservationConsole", (7313, 1150, 0), "-X")
    blocker_for(obs, steel, block_visibility=False)
    cont = place_kit("Console", "A07K_ContainmentConsole", (10650, 0, 0), "+X")
    blocker_for(cont, steel, block_visibility=False)
    for label, loc in (("A07_ObservationConsole", (7244, 1150, 120)), ("A07_ContainmentConsole", (10719, 0, 120))):
        point = LEVEL.get(label)
        if point is not None and kit_mesh("Console") is not None:
            point.set_actor_location(unreal.Vector(*loc), False, False)
            set_prop(point, ["show_panel", "b_show_panel"], False)

    # decon pod along the airlock's north wall, porthole to the room
    blocker_for(place_kit("DeconPod", "A07K_DeconPod", (5350, 456, 0), "-Y"), steel)

    # wall cladding: chamber north (skip the two shelves), south (skip the damaged wall), east (skip door + cells)
    def panels(prefix, coords, facing, scale=1.0):
        for i, (x, y) in enumerate(coords):
            blocker_for(place_kit("WallPanel", "%s_%02d" % (prefix, i), (x, y, 0), facing, scale), concrete, shrink=0.98)

    d = 46.7 / 2
    north = [(7564 + 300 * k, 1886 - d) for k in range(12) if k not in (1, 10)]
    south = [(7564 + 300 * k, -1886 + d) for k in range(9)]
    east = [(11186 - d, y) for y in (-1736, -1436, -536, -236, 64, 964, 1564)]
    tunnel = [(13352 + 276 * k, -464 - d * 0.92) for k in range(7)]
    panels("A07K_WallPanel_N", north, "-Y")
    panels("A07K_WallPanel_S", south, "+Y")
    panels("A07K_WallPanel_E", east, "-X")
    panels("A07K_WallPanel_Tunnel", tunnel, "-Y", 0.92)

    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    log("done. Walk up the chamber's stairs (west ends of both catwalks), press P to check the NavMesh, Ctrl+S.")


main()
