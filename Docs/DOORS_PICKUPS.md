# Doors and pickups

**Controls:** **F** uses whatever you're looking at (a `[F] ...` prompt shows under the
crosshair). **The torch has moved to T.**

## Doors (`AAHDoor`)
- **Construction:** single or double steel doors built from engine shapes, so they work in any
  level. They use the forest materials when those exist.
- **Opening:** F opens a door away from you and closes it again. Sprinting into it (more than
  1.4x walking speed) **slams** it open.
- **Enemies open them:** walking into a door opens it, and a hunting enemy slams it. So a door
  banging open down the corridor means something's coming.
- **Noise:** opening carries 4.5 m, closing 6.5 m, slamming 16 m and a locked rattle 3.5 m.
- **Blocks sight and sound:** a closed door blocks enemies' eyes and muffles sound, so shutting
  one behind you breaks line of sight.
- **NavMesh:** it runs through unlocked doorways, and a **locked** door blocks it so enemies route
  round. After locking or unlocking one in the editor, use Build > Build Paths.
- **Settings (Details panel):** Width, Height, Double Door, Open Angle, Swing Time, Start Open,
  Locked, Locked Prompt, Enemies Can Open, the noise ranges and materials.

## Pickups (`AAHPickup`)

| Type | Effect | Can't take it when |
|---|---|---|
| Magazine | a loaded 12-round mag into the pouch | the pouch is full (6 spare mags) |
| Bandage | heals 35 over ~6 s ("Bandaging..." on the HUD) | not hurt |
| Battery | adds 70% to the torch battery | the torch is nearly full |

Picking something up rustles: enemies within 2 m hear it.

## Placing them
- **Forest maps:** open `AH_ForestResearch_Small` (or your `AH_ForestResearch_MeshyPass01`) and run
  **Tools > Execute Python Script > `Tools/place_doors_pickups_ue58.py`**, then Ctrl+S. It adds:
  - double doors in all six room doorways and at the entrance
  - 4 magazines (reception desk, storage, lab, and the server room where the Gaunt is)
  - 3 bandages (the two medical beds and security)
  - 2 batteries (generator room, lab)

  It's safe to re-run, and everything goes in the Outliner folder `AnomalyHunter/DoorsPickups`.
- **Any other level:** Place Actors, search **AH Door** / **AH Pickup**, drag one in, and set it up
  in the Details panel. A door's origin is the middle of the doorway at floor level, and its width
  runs along its local Y.

## Test plan (NEA)

| # | Test | Expected |
|---|---|---|
| D1 | Look at a door | "[F] Open door"; F swings it away from you |
| D2 | Sprint at a door and press F | it slams; `ah.DebugNoise 1` shows a 16 m sphere |
| D3 | Close a door between you and a chasing enemy | it loses sight, then opens the door (bang) and keeps hunting |
| D4 | Lock one (Details > Locked), Build Paths | "[F] Locked" rattles; enemies path round |
| P1 | Pick up a magazine | reserve +12, mag count +1; with 6 spare mags: "Mag pouch full" |
| P2 | Take damage, use a bandage | health climbs ~6/s up to +35 |
| P3 | Let the torch run low, take a battery | battery bar jumps up |
