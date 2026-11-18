"""Anomaly Hunter: Meshy forest environment placement pass for Unreal Engine 5.8.

IMPORTANT: First DUPLICATE the existing level (Content Browser -> Duplicate)
  /Game/Maps/AH_ForestResearch_Small -> /Game/Maps/AH_ForestResearch_MeshyPass01
Then OPEN the duplicated map. This script refuses to run in any other level.

Safeguards:
- Never edits the original map, TestArena or project settings.
- Skips already placed Meshy actors, so rerunning does not duplicate them.
- Existing blockout props are HIDDEN (not destroyed) only after their Meshy
  replacement has been successfully placed.
- Disables collision on detailed Meshy meshes for this first layout pass.
- Fits each imported mesh uniformly to an approximate design envelope.
  Inspect orientations and scale in the viewport before gameplay testing.
"""
import math
import unreal

REQUIRED_MAP = "AH_ForestResearch_MeshyPass01"
ASSET_FOLDER = "/Game/ForestResearch/MeshyProps"
AUTO_SAVE = False  # Review the result, then Ctrl+S yourself.
HIDE_REPLACED_BLOCKOUT = True
PREFIX = "AH_Meshy_"

# name, actor label, XYZ center of intended footprint (bottom elevation Z),
# desired max W/D/H (centimetres), yaw degrees, original blockout label prefixes.
# These placements match Tools/build_forest_research_ue58.py's six-room layout.
# Meshy-generated asset identities were inferred from silhouettes; inspect
# every instance before treating the level art as final.
PLACEMENTS = [
    ("SM_FR_StorageCrate", "AH_Meshy_Crate_Storage",
     (620, -780, 0), (100, 100, 100), 0,
     ("FR_Crate_1", "FR_CrateBand_1")),

    ("SM_FR_ResearchWorkstation", "AH_Meshy_Lab_Workstation",
     (1910, 960, 0), (245, 165, 180), 0,
     ("FR_LabBench_1_", "FR_LabTerminal_1_", "FR_LabSampleBox_1")),

    ("SM_FR_IndustrialFixture", "AH_Meshy_SecurityFixture",
     (890, 1250, 0), (110, 100, 175), 0,
     ()),

    ("SM_FR_ExitFixture", "AH_Meshy_ExitSign",
     (100, -5, 244), (140, 25, 65), 0,
     ()),

    ("SM_FR_BiohazardSign", "AH_Meshy_LabWarning",
     (2180, 245, 160), (95, 18, 95), 0,
     ()),

    ("SM_FR_LabDoor", "AH_Meshy_LabDoor_Decorative",
     (2000, 250, 0), (220, 35, 265), 0,
     ()),

    ("SM_FR_Generator", "AH_Meshy_Generator_01",
     (2960, -1020, 0), (310, 190, 205), 0,
     ("FR_GeneratorFrame_0", "FR_GeneratorCore_0",
      "FR_GeneratorStatus_0", "FR_GeneratorVent_0_")),

    ("SM_FR_RSUWallSign", "AH_Meshy_RSU_EntranceSign",
     (-35, 310, 195), (22, 180, 86), 90,
     ()),

    ("SM_FR_IndustrialBarrier", "AH_Meshy_EntranceBarrier",
     (-650, 375, 0), (225, 65, 135), 0,
     ()),

    ("SM_FR_FloorGrate", "AH_Meshy_CorridorGrate_A",
     (120, -158, 2), (115, 60, 8), 0,
     ("FR_Grate_120", "FR_GrateSlot_120_")),

    ("SM_FR_FloorGrate", "AH_Meshy_CorridorGrate_B",
     (1940, -158, 2), (115, 60, 8), 0,
     ("FR_Grate_1940", "FR_GrateSlot_1940_")),

    ("SM_FR_IndustrialShelf", "AH_Meshy_StorageShelf",
     (300, -950, 0), (180, 58, 245), 0,
     ("FR_StorageShelf_0_",)),
]

library = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def log(msg):
    unreal.log("[AH Meshy Placement] " + str(msg))

def warn(msg):
    unreal.log_warning("[AH Meshy Placement] " + str(msg))

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
    spans = (float(high.x - low.x),
             float(high.y - low.y),
             float(high.z - low.z))
    mids = (float(low.x + high.x) * 0.5,
            float(low.y + high.y) * 0.5)
    min_z = float(low.z)
    if min(spans) <= 0.00001 or max(spans) > 100000000:
        raise ValueError("Unexpected mesh bounds: " + str(spans))
    return spans, mids, min_z

def new_actor(mesh, label, position, envelope, yaw):
    spans, mids, min_z = dimensions(mesh)
    rad = math.radians(yaw)
    cosine, sine = math.cos(rad), math.sin(rad)
    # Yaw rotates the asset's X-Y bounding-box extents. Use uniform scaling.
    footprint_x = abs(cosine) * spans[0] + abs(sine) * spans[1]
    footprint_y = abs(sine) * spans[0] + abs(cosine) * spans[1]
    factors = (envelope[0] / max(0.01, footprint_x),
               envelope[1] / max(0.01, footprint_y),
               envelope[2] / max(0.01, spans[2]))
    factor = min(factors)
    if not (0.0001 <= factor <= 1000.0):
        raise ValueError("Unsafe auto-scale " + str(factor))
    rx = mids[0] * cosine - mids[1] * sine
    ry = mids[0] * sine + mids[1] * cosine
    origin = unreal.Vector(position[0] - rx * factor,
                           position[1] - ry * factor,
                           position[2] - min_z * factor)
    rot = unreal.Rotator(pitch=0, yaw=yaw, roll=0)
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, origin, rot)
    if not a:
        raise RuntimeError("Editor couldn't spawn the actor")
    try:
        a.set_actor_label(label)
        a.static_mesh_component.set_static_mesh(mesh)
        a.set_actor_scale3d(unreal.Vector(factor, factor, factor))
        a.set_actor_enable_collision(False)
        a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        a.set_folder_path(unreal.Name("AnomalyHunter/MeshyPass01"))
        log("%s -> envelope %s cm | imported bounds %s cm | scale %.4f"
            % (label, envelope, tuple(round(v, 1) for v in spans), factor))
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
        if any(name.startswith(p) for p in prefixes):
            a.set_actor_hidden_in_game(True)
            a.set_actor_enable_collision(False)
            a.set_is_temporarily_hidden_in_editor(True)
            hidden += 1
    log("Disabled %d corresponding blockout actors" % hidden)

def main():
    if not active_map_is_safe():
        return
    existing = {a.get_actor_label() for a in actors.get_all_level_actors()}
    placed, skipped, missing, failed = 0, 0, 0, 0
    for name, label, position, envelope, yaw, replaced in PLACEMENTS:
        if label in existing:
            log("Already present: " + label)
            skipped += 1
            continue
        asset_path = ASSET_FOLDER + "/" + name
        mesh = library.load_asset(asset_path)
        if not mesh or not isinstance(mesh, unreal.StaticMesh):
            warn("Missing Static Mesh %s; skipped" % asset_path)
            missing += 1
            continue
        try:
            new_actor(mesh, label, position, envelope, yaw)
            hide_matching_blockout(replaced)
            existing.add(label)
            placed += 1
        except Exception as exc:
            unreal.log_error("[AH Meshy Placement] " + label + ": " + str(exc))
            failed += 1
    log("Finished: placed=%d, existing=%d, missing=%d, errors=%d."
        % (placed, skipped, missing, failed))
    if AUTO_SAVE and placed:
        try:
            if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level():
                log("Saved duplicate map: " + REQUIRED_MAP)
            else:
                warn("Save returned false. Save the map manually.")
        except Exception as exc:
            warn("Could not autosave map: " + str(exc))
    else:
        log("Review scale, placement, labels and clearance in viewport, then Ctrl+S.")
    log("No new meshes have collision. Add simple collision manually after visual review.")

main()
