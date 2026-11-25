# A-07 expanded map: playable mission

`AH_ForestResearch_A07_Expanded` as a level you can play start to finish: one mission, ~10-15 minutes,
with an anomaly in the original facility and a breach set piece in the new wing.

## Setup (once)

1. Build the C++ (Visual Studio, Development Editor) and open the editor.
2. The 7 new WAVs are already in `Assets/Audio` (no need to run generate_assets.py); the setup
   script below imports them into /Game/Generated/Audio and makes `SW_Alarm_Loop` loop.
3. If you haven't built the map yet: duplicate `AH_ForestResearch_MeshyPass01` as
   `AH_ForestResearch_A07_Expanded`, open it, run `Tools/build_a07_expanded_ue58.py`.
4. With that map open, run **`Tools/setup_a07_playable_ue58.py`**. Then press **P** in the viewport:
   the green NavMesh should run through every doorway. **Ctrl+S**.

Re-running either script is safe (labels that exist are updated, not duplicated).

## The mission

| # | Where | Device | What happens |
|---|-------|--------|--------------|
| 1 | Storage (south-west room) | lockbox + keycard | card taken |
| 2 | Generator room | breaker cabinet | the wing's lights stutter on; the generator is loud (enemies hear it, 18 m) |
| 3 | Rear corridor | card reader beside the slab | the security door grinds up into the ceiling |
| 4 | Airlock | yellow decon panel | 5 s spray cycle, then the inner airlock door unlocks and swings open |
| 5 | Observation room | terminal (or the Meshy console) | three log entries |
| 6 | A-07 chamber, east of the vessel | containment console | **breach**: alarm, wing power dies, red lights pulse, a Gaunt is let out at the chamber's west end and already knows roughly where you are |
| 7 | End of the maintenance tunnel | radio beacon | extraction: the sealed exit opens, end screen with time + checkpoints used, level restarts after 12 s |

- Use everything with **F** (the `[F] ...` prompt). **G** works too.
- Each device's light: red = not yet, **pulsing amber = do this one now**, green = done.
  In a dark room the amber pulse is how you find it.
- Locked steps tell you why: `[F] Restore backup power  -  needs maintenance credentials`.
- The objective is top left; mission messages appear above the crosshair.

## Checkpoints

Die and the level reloads (as before) but you come back where you finished the last step, with that
progress kept (lights on, door up, airlock open, breach still going). They live in a GameInstance
subsystem (`UAHMissionProgress`), so a new Play-In-Editor session always starts from the beginning.
Turn off with `bUseCheckpoints` on the director.

## What the setup script places

- Original facility: the 7 doors + 9 pickups from `place_doors_pickups_ue58.py`, and a live Gaunt in
  the server room if the level only has the visual-only stand-in (the stand-in is hidden, not deleted).
- A-07 doors: airlock inner (locked until decon), observation, chamber west/east, maintenance east,
  tunnel exit (sealed until extraction). Enemies open unlocked doors, so the Gaunt follows you.
- A-07 pickups: 4 magazines, 2 bandages, 2 torch batteries along the route.
- Light roles by tag: `A07_Red*` = `AH_EmergencyLight` (pulse in the breach); a few hall lights and the
  tunnel exit light = `AH_FlickerLight`. Tagged lights are made Movable (Static lights can't change).
- The security slab is resized to fill the corridor, made Movable and kept out of the NavMesh.
- A pad + moonlight outside the tunnel exit so the opened door doesn't look into a void.

## Tuning (select `A07_MissionDirector` in the Outliner, Details > Mission)

| Setting | Default | |
|---|---|---|
| Spawn Gaunt After Containment | on | the breach Gaunt |
| Gaunt Spawn Location | 7900, -1350, 110 | chamber, west end |
| Decon Duration | 5 s | |
| Restricted Door Lift / Open Time | 310 cm / 2.6 s | |
| Alarm Range | 6000 cm | who hears the breach |
| Restart Delay | 12 s | 0 = stay on the end screen |
| Use Checkpoints | on | |
| Debug Messages | off | old GEngine on-screen text as well as the HUD |

Devices (`A07_*` interaction points): `Show Panel` (off = invisible but still usable, e.g. over a Meshy
model), `Panel Offset`, `Panel Yaw` (which way the screen faces), `Pedestal`.

## Sounds (Tools/generate_assets.py, build_facility_audio, seed 2612)

`SW_Terminal_Accept_01`, `SW_Terminal_Deny_01`, `SW_Power_On_01`, `SW_BlastDoor_Open_01`,
`SW_Decon_Hiss_01`, `SW_Alarm_Loop`, `SW_Light_Pop_01`. Missing ones are skipped silently.

## Playtest checklist

- [ ] Start in the forest, objective top left says find the access card
- [ ] Storage: amber light pulsing on the lockbox, `[F] Take maintenance access card`, card disappears
- [ ] Power before the card: deny buzz + "needs maintenance credentials"
- [ ] Generator: lights in the wing flicker on; the server-room Gaunt may come to look
- [ ] Security door lifts, you can walk under it about 2/3 of the way up
- [ ] Inner airlock door: "Locked - decontamination required" until the cycle finishes, then opens
- [ ] Observation logs show one after another
- [ ] Breach: alarm, lights die, red pulse, Gaunt comes from the west of the chamber
- [ ] Die after the breach: you wake at the containment console, alarm still going, Gaunt back
- [ ] Beacon: tunnel exit opens, end screen, restart after 12 s
