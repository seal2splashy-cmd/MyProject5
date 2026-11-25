"""
setup_a07_playable_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...)
with /Game/Maps/AH_ForestResearch_A07_Expanded OPEN, after Tools/build_a07_expanded_ue58.py
and after the C++ has been built (it needs AHDoor, AHPickup, FacilityInteractionPoint,
FacilityMissionDirector and GauntAnomaly).

Turns the A-07 blockout into a playable level:
  1. doors + pickups in the original facility (runs Tools/place_doors_pickups_ue58.py)
  2. A-07 doors: the airlock's inner door (locked until decontamination finishes), observation,
     chamber in/out, maintenance, and the sealed tunnel exit that opens when you call extraction
  3. A-07 pickups along the route: magazines, bandages, torch batteries
  4. mission devices: labels, which way each one faces, the observation terminal hidden when the
     Meshy control console is there (F still works on it)
  5. the security door slab: fills the corridor, can move, doesn't cut the NavMesh
  6. light roles: A07_Red* pulse in the breach (AH_EmergencyLight), a few lights flicker (AH_FlickerLight)
  7. the mission director lets a Gaunt out of the chamber when you inspect A-07
  8. a live Gaunt in the server room if the level only has the visual-only stand-in
  9. a small pad + light outside the tunnel exit, NavMesh rebuilt
 10. imports the mission sounds (Assets/Audio/SW_Terminal_*, SW_Power_On_01, SW_Alarm_Loop, ...)
     into /Game/Generated/Audio if they aren't there yet

Safe to re-run: things already there (same label) are updated, not duplicated. Everything new goes
in the Outliner folder AnomalyHunter/A07_Playable. NOT saved for you: look around, then Ctrl+S.
Docs/A07_PLAYABLE.md
"""

import os
import random
import runpy
import unreal

TARGET_WORLD = "AH_ForestResearch_A07_Expanded"
FOLDER = unreal.Name("AnomalyHunter/A07_Playable")

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
lib = unreal.EditorAssetLibrary

# label, (x, y, floor z), yaw, width, height, tags, locked prompt (None = unlocked)
# yaw 0 = a doorway in a wall running along Y, yaw 90 = in a wall running along X
DOORS = [
    ("A07_Door_AirlockInner", (6000, 0, 0), 0, 360, 266, ["AH_DeconLockedDoor"],
     "Locked  -  decontamination required"),
    ("A07_Door_Observation", (6700, 550, 0), 90, 350, 266, [], None),
    ("A07_Door_ChamberWest", (7400, 0, 0), 0, 370, 266, [], None),
    ("A07_Door_ChamberEast", (11200, -900, 0), 0, 340, 266, [], None),
    ("A07_Door_MaintenanceEast", (13200, -900, 0), 0, 340, 266, [], None),
    ("A07_Door_TunnelExit", (15300, -900, 0), 0, 340, 266, ["AH_ExtractionDoor"],
     "Sealed  -  signal extraction first"),
]

# label, type, (x, y, z of the surface)
PICKUPS = [
    ("A07_Pickup_Mag_Airlock", "MAGAZINE", (4850, -460, 0)),
    ("A07_Pickup_Bandage_Airlock", "BANDAGE", (5900, -470, 0)),
    ("A07_Pickup_Battery_Observation", "BATTERY", (6400, 1760, 0)),
    ("A07_Pickup_Mag_Observation", "MAGAZINE", (7250, 700, 0)),
    ("A07_Pickup_Mag_Chamber", "MAGAZINE", (10900, 1500, 0)),
    ("A07_Pickup_Bandage_Maintenance", "BANDAGE", (12000, -1460, 0)),
    ("A07_Pickup_Mag_Maintenance", "MAGAZINE", (12800, -420, 0)),
    ("A07_Pickup_Battery_Tunnel", "BATTERY", (14200, -560, 0)),
]

# mission device label -> (HUD label, panel yaw: which way its screen faces)
DEVICES = {
    "A07_MaintenanceAccessCard": ("Maintenance access card", 90.0),
    "A07_BackupPower": ("Backup power breaker", 90.0),
    "A07_SecurityDoor": ("Security door reader", 0.0),      # auto-placed beside the doorway
    "A07_Decontamination": ("Decontamination control", 90.0),
    "A07_ObservationConsole": ("Observation terminal", 180.0),
    "A07_ContainmentConsole": ("Containment console", 180.0),
    "A07_EmergencyExtraction": ("Extraction beacon", 180.0),
}

MISSION_SOUNDS = ["SW_Terminal_Accept_01", "SW_Terminal_Deny_01", "SW_Power_On_01", "SW_BlastDoor_Open_01",
                  "SW_Decon_Hiss_01", "SW_Alarm_Loop", "SW_Light_Pop_01"]
AUDIO_DST = "/Game/Generated/Audio"

FLICKER_LIGHTS = ["FR_HallLight_2050", "FR_HallLight_3250", "FR_MaintenanceLamp", "A07_RedTunnelExit"]
GAUNT_RELEASE = (7900.0, -1350.0, 110.0)


def log(msg):
    unreal.log("[A07 Playable] " + str(msg))


def warn(msg):
    unreal.log_warning("[A07 Playable] " + str(msg))


def set_prop(obj, names, value, quiet=False):
    """Unreal Python drops the b of bools (bLocked -> locked), so try a few spellings."""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            continue
    if not quiet:
        warn("couldn't set %s on %s" % (names[0], obj.get_name()))
    return False


def add_tags(actor, tags):
    try:
        current = list(actor.get_editor_property("tags"))
    except Exception:
        current = []
    changed = False
    for t in tags:
        if unreal.Name(t) not in current:
            current.append(unreal.Name(t))
            changed = True
    if changed:
        set_prop(actor, ["tags"], current)


def light_component(actor):
    return actor.get_component_by_class(unreal.LightComponent) if actor else None


def make_movable(actor):
    root = actor.get_editor_property("root_component") if actor else None
    if root:
        try:
            root.set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception as exc:
            warn("couldn't make %s movable: %s" % (actor.get_actor_label(), exc))


def by_label():
    return {a.get_actor_label(): a for a in actors.get_all_level_actors() if a}


def spawn(cls, label, xyz, yaw=0.0):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*xyz), unreal.Rotator(0, 0, yaw))
    if not a:
        warn("couldn't spawn " + label)
        return None
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    return a


def tools_dir():
    try:
        return os.path.dirname(os.path.abspath(__file__))
    except NameError:
        return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Tools")


def import_mission_sounds():
    src = os.path.join(os.path.dirname(tools_dir()), "Assets", "Audio")
    tasks = []
    for name in MISSION_SOUNDS:
        if lib.does_asset_exist(AUDIO_DST + "/" + name):
            continue
        wav = os.path.join(src, name + ".wav")
        if not os.path.isfile(wav):
            warn("missing %s (git pull, or run python Tools/generate_assets.py)" % wav)
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", wav)
        t.set_editor_property("destination_path", AUDIO_DST)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        log("imported %d mission sounds" % len(tasks))
    alarm = lib.load_asset(AUDIO_DST + "/SW_Alarm_Loop") if lib.does_asset_exist(AUDIO_DST + "/SW_Alarm_Loop") else None
    if alarm:
        set_prop(alarm, ["looping"], True)
        lib.save_loaded_asset(alarm)


def main():
    world = editor.get_editor_world()
    name = world.get_name() if world else "(none)"
    if name != TARGET_WORLD:
        unreal.log_error("[A07 Playable] open %s first (you have %s). Build it with "
                         "Tools/build_a07_expanded_ue58.py." % (TARGET_WORLD, name))
        return

    classes = {}
    for key, path in (("door", "/Script/MyProject5.AHDoor"), ("pickup", "/Script/MyProject5.AHPickup"),
                      ("point", "/Script/MyProject5.FacilityInteractionPoint"),
                      ("director", "/Script/MyProject5.FacilityMissionDirector"),
                      ("gaunt", "/Script/MyProject5.GauntAnomaly")):
        classes[key] = unreal.load_class(None, path)
    missing = [k for k, v in classes.items() if v is None]
    if missing:
        unreal.log_error("[A07 Playable] C++ classes missing (%s) - build the project in Visual Studio, "
                         "reopen the editor, then run this again." % ", ".join(missing))
        return

    level = by_label()
    if "A07_MissionDirector" not in level:
        unreal.log_error("[A07 Playable] no A07_MissionDirector in this level - run "
                         "Tools/build_a07_expanded_ue58.py first (with the C++ built).")
        return

    import_mission_sounds()

    # 1. the original facility's doors and pickups
    try:
        runpy.run_path(os.path.join(tools_dir(), "place_doors_pickups_ue58.py"), run_name="__main__")
    except Exception as exc:
        warn("place_doors_pickups_ue58.py failed (%s) - run it by hand" % exc)
    level = by_label()

    # 2. A-07 doors
    meshy_entrance = level.get("A07_MeshyContainmentEntrance")
    doors_new = 0
    for label, xyz, yaw, width, height, tags, locked_prompt in DOORS:
        door = level.get(label)
        if door is None:
            door = spawn(classes["door"], label, xyz, yaw)
            if door is None:
                continue
            doors_new += 1
        set_prop(door, ["width"], float(width))
        set_prop(door, ["height"], float(height))
        set_prop(door, ["double_door", "b_double_door", "bDoubleDoor"], True)
        if label == "A07_Door_ChamberWest" and meshy_entrance is not None:
            # the Meshy containment entrance is already the frame
            set_prop(door, ["show_frame", "b_show_frame", "bShowFrame"], False)
        if locked_prompt:
            set_prop(door, ["locked", "b_locked", "bLocked"], True)
            set_prop(door, ["locked_prompt"], locked_prompt)
        add_tags(door, tags)

    # 3. A-07 pickups
    rnd = random.Random(707)
    enum = getattr(unreal, "AHPickupType", None)
    pickups_new = 0
    for label, kind, xyz in PICKUPS:
        if label in level:
            continue
        item = spawn(classes["pickup"], label, xyz, rnd.uniform(0, 360))
        if item is None:
            continue
        if enum is not None:
            set_prop(item, ["type"], getattr(enum, kind))
        pickups_new += 1

    # 4. mission devices
    console_mesh = level.get("A07_MeshyControlConsole")
    has_meshy_console = bool(console_mesh and console_mesh.static_mesh_component.get_editor_property("static_mesh"))
    for label, (nice, yaw) in DEVICES.items():
        point = level.get(label)
        if point is None:
            warn("mission device %s isn't in the level (rerun build_a07_expanded_ue58.py)" % label)
            continue
        set_prop(point, ["label"], nice)
        set_prop(point, ["panel_yaw"], float(yaw))
        if label == "A07_ObservationConsole":
            set_prop(point, ["show_panel", "b_show_panel", "bShowPanel"], not has_meshy_console)
        # set_editor_property already re-runs the construction (so the device rebuilds in the viewport)

    # 5. the security door slab
    slab = level.get("A07_RestrictedVisualDoor")
    if slab is not None:
        slab.set_actor_location(unreal.Vector(4600, 0, 165), False, False)
        slab.set_actor_scale3d(unreal.Vector(0.38, 4.36, 3.30))     # 38 x 436 x 330 cm: wall to wall
        make_movable(slab)
        try:
            slab.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
        except Exception as exc:
            warn("slab navigation flag: %s" % exc)
    else:
        warn("A07_RestrictedVisualDoor missing - the security door won't have a visible slab")

    # 6. light roles
    for label, a in level.items():
        comp = light_component(a)
        if comp is None:
            continue
        if label.startswith("A07_Red") and label not in FLICKER_LIGHTS:
            add_tags(a, ["AH_EmergencyLight"])
            make_movable(a)
        elif label in FLICKER_LIGHTS:
            add_tags(a, ["AH_FlickerLight"])
            make_movable(a)
        else:
            try:
                if unreal.Name("AH_PoweredLight") in list(a.get_editor_property("tags")):
                    make_movable(a)
            except Exception:
                pass

    # 7. the director
    director = level["A07_MissionDirector"]
    set_prop(director, ["spawn_gaunt_after_containment", "b_spawn_gaunt_after_containment"], True)
    set_prop(director, ["gaunt_spawn_location"], unreal.Vector(*GAUNT_RELEASE))
    set_prop(director, ["show_prototype_hud", "b_show_prototype_hud"], True, quiet=True)

    # 8. a live Gaunt in the server room
    gaunts = [a for a in level.values() if isinstance(a, classes["gaunt"])]
    if not gaunts:
        g = spawn(classes["gaunt"], "FR_Anomaly_Gaunt", (3500, 650, 100), 180.0)
        if g:
            log("added a live Gaunt to the server room")
        stand_in = level.get("FR_Anomaly_VisualOnly")
        if stand_in is not None:
            stand_in.set_actor_hidden_in_game(True)
            stand_in.set_is_temporarily_hidden_in_editor(True)
            log("hid FR_Anomaly_VisualOnly (the live Gaunt replaces it; delete it if you like)")

    # 9. outside the tunnel exit: somewhere to stand + some moonlight, so the open door isn't a void
    if "A07_ExitPad" not in level:
        cube = lib.load_asset("/Engine/BasicShapes/Cube")
        pad = spawn(unreal.StaticMeshActor, "A07_ExitPad", (15800, -900, -15))
        if pad and cube:
            pad.static_mesh_component.set_static_mesh(cube)
            pad.set_actor_scale3d(unreal.Vector(10.0, 8.0, 0.3))
            mat = lib.load_asset("/Game/ForestResearch/Materials/M_FR_ForestSoil")
            if mat:
                pad.static_mesh_component.set_material(0, mat)
    if "A07_ExitMoonlight" not in level:
        moon = spawn(unreal.PointLight, "A07_ExitMoonlight", (15900, -900, 320))
        comp = light_component(moon)
        if comp:
            comp.set_intensity(900.0)
            comp.set_attenuation_radius(900.0)
            comp.set_light_color(unreal.LinearColor(0.55, 0.65, 0.8, 1.0))

    # NavMesh: the builder's A07_ExtendedNavBounds covers the wing; make sure it exists, then rebuild
    level = by_label()
    if "A07_ExtendedNavBounds" not in level:
        nav = spawn(unreal.NavMeshBoundsVolume, "A07_ExtendedNavBounds", (9500, 0, 425))
        if nav:
            nav.set_actor_scale3d(unreal.Vector(61, 27, 5.5))
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")

    log("done: %d new doors, %d new pickups, devices/lights/director updated, NavMesh rebuilding." % (doors_new, pickups_new))
    log("Press P in the viewport to check the green NavMesh runs through every doorway, then Ctrl+S.")


main()
