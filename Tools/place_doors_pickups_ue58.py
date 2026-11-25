"""
place_doors_pickups_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...)
with a forest facility map open: AH_ForestResearch_Small, AH_ForestResearch_MeshyPass01 or
AH_ForestResearch_A07_Expanded (Tools/setup_a07_playable_ue58.py runs this one for you).

Adds:
  - double doors (AAHDoor) in the six room doorways off the corridor and at the entrance
  - pickups (AAHPickup): 9 mm magazines, bandages and torch batteries, in the rooms where
    you'd expect them (ammo at reception / storage / server room, bandages in medical...)

Safe to re-run: anything already placed (same label) is skipped. Everything goes in the
Outliner folder AnomalyHunter/DoorsPickups, so it's easy to select and delete. Save with Ctrl+S.
Other levels: drag "AH Door" / "AH Pickup" in from Place Actors (search "AH").
Docs/DOORS_PICKUPS.md
"""

import random
import unreal

MAPS = ("AH_ForestResearch_Small", "AH_ForestResearch_MeshyPass01", "AH_ForestResearch_A07_Expanded")
FOLDER = "AnomalyHunter/DoorsPickups"

# label, (x, y, z floor), yaw, width, height. Yaw 90 = a door in a wall running along X.
DOORS = [
    ("FR_Door_Reception", (720, 205, 0), 90, 236, 230),
    ("FR_Door_Lab", (2000, 205, 0), 90, 236, 230),
    ("FR_Door_Server", (3360, 205, 0), 90, 236, 230),
    ("FR_Door_Storage", (720, -205, 0), 90, 236, 230),
    ("FR_Door_Medical", (2000, -205, 0), 90, 236, 230),
    ("FR_Door_Generator", (3360, -205, 0), 90, 236, 230),
    ("FR_Door_Entrance", (0, 0, 0), 0, 296, 222),
]
# Labels of doors to start locked (a locked door blocks the NavMesh: Build > Build Paths after).
LOCKED = set()

# label, type, (x, y, z of the surface it rests on)
PICKUPS = [
    ("FR_Pickup_Mag_Reception", "MAGAZINE", (590, 900, 88)),      # on the reception desk
    ("FR_Pickup_Mag_Storage", "MAGAZINE", (460, -600, 0)),
    ("FR_Pickup_Mag_Server", "MAGAZINE", (3300, 880, 0)),         # where the Gaunt lurks
    ("FR_Pickup_Mag_Lab", "MAGAZINE", (1700, 650, 0)),
    ("FR_Pickup_Bandage_Medical1", "BANDAGE", (1630, -1040, 98)), # on the observation beds
    ("FR_Pickup_Bandage_Medical2", "BANDAGE", (2300, -1040, 98)),
    ("FR_Pickup_Bandage_Security", "BANDAGE", (300, 1100, 0)),
    ("FR_Pickup_Battery_Generator", "BATTERY", (3150, -640, 0)),
    ("FR_Pickup_Battery_Lab", "BATTERY", (2150, 620, 0)),
]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log("[Doors+Pickups] " + msg)


def set_prop(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            continue
    unreal.log_warning("[Doors+Pickups] couldn't set %s on %s" % (names[0], obj.get_actor_label()))
    return False


def pickup_type(name):
    enum = getattr(unreal, "AHPickupType", None)
    return getattr(enum, name) if enum else None


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None or world.get_name() not in MAPS:
        unreal.log_error("[Doors+Pickups] open %s first (this one is %s). Other maps: place AH Door / AH Pickup "
                         "by hand from Place Actors." % (" or ".join(MAPS), world.get_name() if world else "none"))
        return

    door_cls = unreal.load_class(None, "/Script/MyProject5.AHDoor")
    pickup_cls = unreal.load_class(None, "/Script/MyProject5.AHPickup")
    if not door_cls or not pickup_cls:
        unreal.log_error("[Doors+Pickups] AHDoor / AHPickup classes not found - build the C++ first")
        return

    existing = {a.get_actor_label() for a in actors.get_all_level_actors()}
    placed = 0

    for label, (x, y, z), yaw, width, height in DOORS:
        if label in existing:
            continue
        door = actors.spawn_actor_from_class(door_cls, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
        if not door:
            continue
        door.set_actor_label(label)
        set_prop(door, ["width"], float(width))
        set_prop(door, ["height"], float(height))
        set_prop(door, ["double_door", "b_double_door", "bDoubleDoor"], True)
        if label in LOCKED:
            set_prop(door, ["locked", "b_locked", "bLocked"], True)
        door.set_folder_path(unreal.Name(FOLDER))
        placed += 1

    rnd = random.Random(7)
    for label, kind, (x, y, z) in PICKUPS:
        if label in existing:
            continue
        item = actors.spawn_actor_from_class(pickup_cls, unreal.Vector(x, y, z), unreal.Rotator(0, 0, rnd.uniform(0, 360)))
        if not item:
            continue
        item.set_actor_label(label)
        value = pickup_type(kind)
        if value is not None:
            set_prop(item, ["type"], value)
        item.set_folder_path(unreal.Name(FOLDER))
        placed += 1

    log("%s: placed %d (doors %d, pickups %d in the level script's list; already there skipped). "
        "Check them, then Ctrl+S." % (world.get_name(), placed, len(DOORS), len(PICKUPS)))


main()
