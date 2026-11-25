"""
add_navmesh_ue58.py - run INSIDE the Unreal Editor (Tools > Execute Python Script...)
with the level you want to fix OPEN (TestArena, AH_ForestResearch_Small, a duplicate, ...).

Adds a NavMeshBoundsVolume covering everything in the level, builds the NavMesh and saves the
level. After that AEnemy / ASmartEnemy / AGauntAnomaly path round walls instead of walking
straight at you. Press P in the viewport to see the NavMesh (green = walkable).

Safe to re-run: it resizes its own volume (label "NavMeshBounds_Auto") instead of adding another.
Levels made by Tools/ue_setup_project.py and Tools/build_forest_research_ue58.py after this
change already have one.
"""

import unreal

LABEL = "NavMeshBounds_Auto"
MARGIN = 200.0          # cm of slack around the level on every side
MAX_HALF_SIZE = 40000.0  # ignore anything absurdly big (sky spheres, giant fog volumes)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(msg):
    unreal.log("[NavMesh] " + msg)


def level_bounds():
    """Box around every static mesh in the level (walls, floors, props)."""
    lo = [1e18, 1e18, 1e18]
    hi = [-1e18, -1e18, -1e18]
    found = 0
    for a in actors.get_all_level_actors():
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        origin, extent = a.get_actor_bounds(False)
        if max(extent.x, extent.y, extent.z) > MAX_HALF_SIZE or max(extent.x, extent.y, extent.z) <= 0:
            continue
        for i, (o, e) in enumerate(((origin.x, extent.x), (origin.y, extent.y), (origin.z, extent.z))):
            lo[i] = min(lo[i], o - e)
            hi[i] = max(hi[i], o + e)
        found += 1
    return (lo, hi) if found else (None, None)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None:
        unreal.log_error("[NavMesh] no level open")
        return

    lo, hi = level_bounds()
    if lo is None:
        unreal.log_error("[NavMesh] no static meshes in %s - nothing to walk on" % world.get_name())
        return
    centre = unreal.Vector(*[(l + h) * 0.5 for l, h in zip(lo, hi)])
    half = [(h - l) * 0.5 + MARGIN for l, h in zip(lo, hi)]

    vol = next((a for a in actors.get_all_level_actors()
                if isinstance(a, unreal.NavMeshBoundsVolume) and a.get_actor_label() == LABEL), None)
    others = [a for a in actors.get_all_level_actors()
              if isinstance(a, unreal.NavMeshBoundsVolume) and a.get_actor_label() != LABEL]
    if others:
        log("the level already has %d NavMeshBoundsVolume(s) of its own - adding mine as well "
            "(delete either if you don't want both)" % len(others))
    if vol is None:
        vol = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, centre)
        if vol is None:
            unreal.log_error("[NavMesh] couldn't spawn a NavMeshBoundsVolume")
            return
        vol.set_actor_label(LABEL)
    vol.set_actor_location(centre, False, False)
    # the editor gives a new volume a 200 cm cube brush, so scale = half-size / 100
    vol.set_actor_scale3d(unreal.Vector(half[0] / 100.0, half[1] / 100.0, half[2] / 100.0))

    _, ext = vol.get_actor_bounds(False)
    if ext.x < 1.0:
        unreal.log_error("[NavMesh] the volume came out with no size (no brush). Place one by hand: "
                         "Place Actors > Volumes > Nav Mesh Bounds Volume, scale it over the level.")
        return

    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    levels.save_current_level()
    log("%s: NavMesh bounds %.0f x %.0f x %.0f m, built and saved. Press P in the viewport to see it."
        % (world.get_name(), half[0] * 0.02, half[1] * 0.02, half[2] * 0.02))


main()
