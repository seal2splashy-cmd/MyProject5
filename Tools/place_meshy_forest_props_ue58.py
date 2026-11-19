"""Anomaly Hunter: Meshy forest environment placement pass for Unreal Engine 5.8.

IMPORTANT: First DUPLICATE the existing level (Content Browser -> Duplicate)
  /Game/Maps/AH_ForestResearch_Small -> /Game/Maps/AH_ForestResearch_MeshyPass01
Then OPEN the duplicated map. This script refuses to run in any other level.

Safeguards:
- Never edits the original map, TestArena or project settings.
- Skips already placed Meshy actors, so rerunning does not duplicate them.
- Existing blockout props are HIDDEN (not destroyed) only after their Meshy replacement has
  been placed. Their collision is KEPT, so you still bump into the crates, shelves, desks and
  generators while the dense Meshy meshes themselves have no collision.
- Nothing is placed in a doorway or on the walkable route.

Sizes and positions: Meshy scales every model so its longest side is ~1.9 m, so each placement
gives a real-world size envelope (cm) and the mesh is scaled uniformly to fit it. Positions come
from Tools/build_forest_research_ue58.py: building x 0..4000, y -1500..1500, ceiling at z 320,
corridor y -191..191, rooms north (reception / lab / server) and south (storage / medical /
generator), doorways at x 720 / 2000 / 3360.

Facing: every model is turned so its front faces the direction given ("+X", "-Y", ...).
The models' fronts (checked by rendering them) are +Y in Unreal as imported, except the exit
sign whose readable face is -Y. If a prop comes out facing the wall, set FRONT_YAW_FIX = 180.
"""
import math
import unreal

REQUIRED_MAP = "AH_ForestResearch_MeshyPass01"
ASSET_FOLDER = "/Game/ForestResearch/MeshyProps"
AUTO_SAVE = False  # Review the result, then Ctrl+S yourself.
HIDE_REPLACED_BLOCKOUT = True
KEEP_BLOCKOUT_COLLISION = True   # hidden blockouts still block the player (Meshy meshes don't)
FRONT_YAW_FIX = 0.0              # 180 if everything faces backwards in your engine build
PREFIX = "AH_Meshy_"
CEILING_Z = 320.0

# Which way each model's front faces in its own (imported) space.
FRONT = {"SM_FR_ExitFixture": "-Y"}          # everything else: "+Y"

# name, actor label, position, envelope (W along the front face, D, H in cm), facing,
# anchor ("floor": position z is the bottom; "ceiling": hangs from CEILING_Z), blockout prefixes
PLACEMENTS = []

def add(name, label, pos, envelope, facing, anchor="floor", replaces=()):
    PLACEMENTS.append((name, label, pos, envelope, facing, anchor, tuple(replaces)))

# --- south-west storage: four shelves, three crate stacks, a duct and a lamp overhead
for i, (x, y) in enumerate(((300, -950), (570, -950), (880, -950), (400, -1290))):
    add("SM_FR_IndustrialShelf", "AH_Meshy_StorageShelf_%d" % i, (x, y, 0), (180, 80, 245), "+Y",
        replaces=("FR_StorageShelf_%d_" % i,))
for i, (x, y, f) in enumerate(((280, -720, "+Y"), (620, -780, "+X"), (850, -1200, "+Y"))):
    add("SM_FR_StorageCrate", "AH_Meshy_CrateStack_%d" % i, (x, y, 0), (95, 70, 90), f,
        replaces=("FR_Crate_%d" % i, "FR_CrateBand_%d" % i))
add("SM_FR_CeilingDuct", "AH_Meshy_Duct_Storage", (650, -560, 0), (260, 40, 90), "+Y", "ceiling")
add("SM_FR_HangingLight", "AH_Meshy_Lamp_Storage", (600, -1120, 0), (120, 30, 95), "+Y", "ceiling")

# --- north lab: the three benches become Meshy workstations, desk front (stool) to the corridor
for i, x in enumerate((1550, 1910, 2300)):
    add("SM_FR_ResearchWorkstation", "AH_Meshy_LabDesk_%d" % i, (x, 960, 0), (170, 145, 130), "-Y",
        replaces=("FR_LabBench_%d_" % i, "FR_LabTerminal_%d_" % i, "FR_LabSampleBox_%d" % i))

# --- south-east generator room: both generators, a duct along the back wall, two lamps
for i, x in enumerate((2960, 3460)):
    add("SM_FR_Generator", "AH_Meshy_Generator_%d" % i, (x, -1020, 0), (300, 150, 200), "+Y",
        replaces=("FR_GeneratorFrame_%d" % i, "FR_GeneratorCore_%d" % i,
                  "FR_GeneratorStatus_%d" % i, "FR_GeneratorVent_%d_" % i))
add("SM_FR_CeilingDuct", "AH_Meshy_Duct_Generator", (3210, -1300, 0), (260, 40, 90), "+Y", "ceiling")
for i, x in enumerate((2960, 3460)):
    add("SM_FR_HangingLight", "AH_Meshy_Lamp_Generator_%d" % i, (x, -700, 0), (120, 30, 95), "+Y", "ceiling")

# --- corridor: every blockout drain grate becomes two Meshy grates, sunk so they sit ~4 cm proud
for x in range(120, 3940, 260):
    for k, dx in enumerate((-29, 29)):
        add("SM_FR_FloorGrate", "AH_Meshy_Grate_%d_%d" % (x, k), (x + dx, -158, -6), (57, 57, 10), "+Y",
            replaces=("FR_Grate_%d" % x, "FR_GrateSlot_%d_" % x))

# --- signs on the corridor side of the walls, beside the doors (wall faces at y = +/-191)
add("SM_FR_BiohazardSign", "AH_Meshy_Biohazard_Lab", (2200, 189, 135), (60, 4, 62), "-Y")
add("SM_FR_BiohazardSign", "AH_Meshy_Biohazard_Medical", (2200, -189, 135), (60, 4, 62), "+Y")
# exit sign on the inside of the entrance header (header inner face x = 16, underside z = 226)
add("SM_FR_ExitFixture", "AH_Meshy_ExitSign", (28, 0, 236), (45, 24, 35), "+X")

# --- locked lab door at the dead end of the corridor (rear wall inner face x = 3986)
add("SM_FR_LabDoor", "AH_Meshy_LockedDoor_CorridorEnd", (3965, 0, 0), (150, 42, 220), "-X")

# --- RSU concrete panels on the outside of the front wall, either side of the entrance
for side, y in (("N", 430), ("S", -430)):
    add("SM_FR_RSUWallPanel", "AH_Meshy_RSUPanel_%s" % side, (-16, y, 0), (180, 4, 194), "-X")


library = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def log(msg):
    unreal.log("[AH Meshy Placement] " + str(msg))

def warn(msg):
    unreal.log_warning("[AH Meshy Placement] " + str(msg))

DIRECTION_ANGLE = {"+X": 0.0, "+Y": 90.0, "-X": 180.0, "-Y": 270.0}

def yaw_for(name, facing):
    """Yaw that turns the model's front to face `facing` (Unreal yaw turns +X towards +Y)."""
    yaw = DIRECTION_ANGLE[facing] - DIRECTION_ANGLE[FRONT.get(name, "+Y")] + FRONT_YAW_FIX
    return (yaw + 180.0) % 360.0 - 180.0

def active_map_is_safe():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if not world or world.get_name() != REQUIRED_MAP:
        actual = world.get_name() if world else "(none)"
        unreal.log_error(
            "[AH Meshy Placement] Refusing to edit '" + actual + "'. "
            "Duplicate AH_ForestResearch_Small as " + REQUIRED_MAP +
            " in Content Browser, open that duplicate, and run again.")
        return False
    return True

def dimensions(mesh):
    box = mesh.get_bounding_box()
    low, high = box.min, box.max
    spans = (float(high.x - low.x), float(high.y - low.y), float(high.z - low.z))
    mids = (float(low.x + high.x) * 0.5, float(low.y + high.y) * 0.5)
    if min(spans) <= 0.00001 or max(spans) > 100000000:
        raise ValueError("Unexpected mesh bounds: " + str(spans))
    return spans, mids, float(low.z)

def new_actor(name, mesh, label, position, envelope, facing, anchor):
    spans, mids, min_z = dimensions(mesh)
    yaw = yaw_for(name, facing)
    # The envelope is in the model's own frame: width along its front face, depth front-to-back.
    # Its front is along local Y, so local X is the width and local Y the depth.
    factor = min(envelope[0] / max(0.01, spans[0]),
                 envelope[1] / max(0.01, spans[1]),
                 envelope[2] / max(0.01, spans[2]))
    if not (0.0001 <= factor <= 1000.0):
        raise ValueError("Unsafe auto-scale " + str(factor))
    rad = math.radians(yaw)
    cosine, sine = math.cos(rad), math.sin(rad)
    rx = mids[0] * cosine - mids[1] * sine          # the mesh's bounds centre after the yaw
    ry = mids[0] * sine + mids[1] * cosine
    bottom = CEILING_Z - spans[2] * factor if anchor == "ceiling" else position[2]
    origin = unreal.Vector(position[0] - rx * factor,
                           position[1] - ry * factor,
                           bottom - min_z * factor)
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, origin,
                                      unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    if not a:
        raise RuntimeError("Editor couldn't spawn the actor")
    try:
        a.set_actor_label(label)
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_scale3d(unreal.Vector(factor, factor, factor))
        a.set_actor_enable_collision(False)
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        a.set_folder_path(unreal.Name("AnomalyHunter/MeshyPass01"))
        log("%s -> %s facing %s, %.0f x %.0f x %.0f cm (scale %.3f)"
            % (label, name, facing, spans[0] * factor, spans[1] * factor, spans[2] * factor, factor))
    except Exception:
        actors.destroy_actor(a)
        raise
    return a

def hide_matching_blockout(prefixes):
    if not HIDE_REPLACED_BLOCKOUT or not prefixes:
        return
    hidden = 0
    for a in actors.get_all_level_actors():
        name = a.get_actor_label()
        if name.startswith(PREFIX):
            continue
        # "FR_Crate_0" = exactly that actor; "FR_LabBench_0_" = every part of that bench
        if any(name == p or (p.endswith("_") and name.startswith(p)) for p in prefixes):
            a.set_actor_hidden_in_game(True)
            a.set_is_temporarily_hidden_in_editor(True)
            if not KEEP_BLOCKOUT_COLLISION:
                a.set_actor_enable_collision(False)
            hidden += 1
    if hidden == 0:
        warn("no blockout matched %s (already hidden, renamed or deleted?)" % (prefixes,))

def main():
    if not active_map_is_safe():
        return
    existing = {a.get_actor_label() for a in actors.get_all_level_actors()}
    placed, skipped, missing, failed = 0, 0, 0, 0
    meshes = {}
    for name, label, position, envelope, facing, anchor, replaced in PLACEMENTS:
        if label in existing:
            skipped += 1
            continue
        if name not in meshes:
            asset = library.load_asset(ASSET_FOLDER + "/" + name)
            meshes[name] = asset if isinstance(asset, unreal.StaticMesh) else None
            if meshes[name] is None:
                warn("Missing Static Mesh %s/%s - import it first; its placements are skipped" % (ASSET_FOLDER, name))
        if meshes[name] is None:
            missing += 1
            continue
        try:
            new_actor(name, meshes[name], label, position, envelope, facing, anchor)
            hide_matching_blockout(replaced)
            existing.add(label)
            placed += 1
        except Exception as exc:
            unreal.log_error("[AH Meshy Placement] " + label + ": " + str(exc))
            failed += 1
    log("Finished: placed=%d, already there=%d, missing mesh=%d, errors=%d (of %d)."
        % (placed, skipped, missing, failed, len(PLACEMENTS)))
    if AUTO_SAVE and placed:
        try:
            if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
                log("Saved duplicate map: " + REQUIRED_MAP)
            else:
                warn("Save returned false. Save the map manually.")
        except Exception as exc:
            warn("Could not autosave map: " + str(exc))
    else:
        log("Review in the viewport (Outliner folder AnomalyHunter/MeshyPass01), then Ctrl+S.")

main()
