# Enemy navigation and line of sight

Enemies (`AEnemy`, `ASmartEnemy`, `AGauntAnomaly`) now **path round walls on a NavMesh** and need
**line of sight** to spot you. The hand-written FSM (Patrol / Chase / Attack / Dead), the
predictive chase and the Q-learning actions are unchanged. They all move through one function,
`AEnemy::MoveTowards`, and that's the only place the movement changed.

## What changed

| | Before | Now |
|---|---|---|
| Moving to a target | straight line, walks into walls | follows a NavMesh path round walls and props; straight line only if the level has no NavMesh or there's no route |
| Patrol points | random point in a circle (often inside a wall or outside the building) | random point it can actually reach on the NavMesh; gives up on a point after 15 s |
| Q-learning flank / retreat / circle targets | could be inside a wall | snapped onto the NavMesh first, then pathed to |
| Spotting the player | anyone within 2500 cm, through walls (`bUseLineOfSight` off) | within 2500 cm **and** a clear view (two rays, to your head and chest; your own gun/arms don't block it) |
| Player ducks out of view in a fight | back to Patrol | Chase: goes to where it last saw you, gives up after 4 s |
| Attacking | anyone within range, even through a wall | only when it can see you; behind a wall it paths round first |

## How it works (for the write-up)

`MoveTowards(Target, StopDist, SpeedScale)` is called every tick by the current state:

1. **Re-planning.** It asks the navigation system for a path when there's no path yet or the
   target has moved more than `RepathDistance` (120 cm), but no more often than every
   `MinRepathInterval` (0.25 s), and it always re-plans every `MaxRepathInterval` (1.5 s) in case
   something moved into the way. In a chase the target moves every tick, so this caps the cost at
   about 4 path queries a second per enemy. A target with no route can't make it query every
   frame either.
2. **Snapping.** The target is first projected onto the NavMesh (`ProjectPointToNavigation`), so a
   flank point inside a wall becomes the nearest walkable spot.
3. **The path.** `FindPathToLocationSynchronously` runs A* over the Recast NavMesh polygons, then
   string-pulls the result into a list of corner points.
4. **Following.** It steers at the next corner and moves on once it's within
   `WaypointAcceptRadius` (60 cm). The stopping distance is measured **along the path**, not in a
   straight line, so being 1 m from the player through a wall doesn't count as arrived.
5. **Fallback.** With no NavMesh, or no route, it walks straight at the target as before and logs
   one warning.

Line of sight: `HasLineOfSightToPlayer` traces two rays (Visibility channel) from the enemy's eyes
(`EyeHeight`) to the player's head and chest. It ignores the enemy itself, the player, and anything
attached to the player (gun, melee weapon), and counts as seen if either ray is clear.

## Setting up a level

- **TestArena / new forest maps:** `Tools/ue_setup_project.py` and `Tools/build_forest_research_ue58.py`
  now add a NavMeshBoundsVolume and build it.
- **Levels you already have** (your existing `AH_ForestResearch_Small`, `AH_ForestResearch_MeshyPass01`,
  anything else): open the level, then **Tools > Execute Python Script > `Tools/add_navmesh_ue58.py`**.
  It covers everything in the level, builds the NavMesh and saves.
- Press **P** in the viewport to see the NavMesh (green = walkable).

## Settings (Details panel on any enemy)

| Setting | Default | |
|---|---|---|
| Enemy > Pathfinding > Use Nav Mesh | on | off = old straight-line movement |
| Enemy > Pathfinding > Draw Debug Path | off | draws the current path in green: good for screenshots |
| Enemy > Detection > Use Line Of Sight | on | off = old radius-only detection |
| Enemy > Patrol > Patrol Give Up Time | 15 s | |
| Repath Distance / Min / Max Repath Interval / Waypoint Accept Radius | 120 cm / 0.25 s / 1.5 s / 60 cm | |

## Test plan (evidence for the NEA)

Turn on **Draw Debug Path** for the enemies you're testing.

| # | Test | Expected |
|---|---|---|
| N1 | Stand on the far side of a wall from a chasing enemy | green path goes round the wall through a gap; the enemy follows it and reaches you |
| N2 | Same, with Use Nav Mesh off | the enemy walks into the wall (the old behaviour, for comparison) |
| N3 | Let an enemy patrol for a minute | it never walks into a wall, and never patrols outside the building |
| N4 | Level with no NavMesh | one "no NavMesh in this level" warning in the Output Log; enemies still move (straight lines) |
| L1 | Stand within 25 m of a patrolling enemy, behind a wall | it doesn't notice you |
| L2 | Step into view | it starts chasing |
| L3 | During a fight, break line of sight round a corner | it goes to where it last saw you, looks for 4 s, then goes back to patrolling |
| L4 | Crouch behind a crate with your head showing | it still sees you (head ray) |
| L5 | Hold the gun out in front of you towards it | your own weapon doesn't hide you |
| A1 | Stand right behind a thin wall from an enemy | it doesn't hit you through the wall; it paths round |
| Q1 | Smart enemy flanks / circles in a corridor | it moves along the corridor instead of pushing into the walls |
