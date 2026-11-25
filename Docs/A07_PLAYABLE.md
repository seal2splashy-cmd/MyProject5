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

5. Then run **`Tools/dress_a07_ue58.py`** (same map open): props, light fittings, hum, reverb - see
   "Dressing" below. P again (the NavMesh now goes round the shelves/crates/generators), Ctrl+S.

Re-running any of the scripts is safe (labels that exist are updated or skipped, not duplicated).

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

## Dressing (Tools/dress_a07_ue58.py)

Uses your imported Meshy assets (`/Game/ForestResearch/MeshyProps/SM_FR_*`, the A-07 modules) - and
if they were imported somewhere else, it finds them by name.

- **Asset scan:** writes every Static Mesh you've imported to `Saved/AH_AssetInventory.txt` and lists
  the Meshy / ForestResearch / A07 folders in the Output Log. If your containment vessel, decon chamber
  or control console are imported under another name or folder (anything with "vessel", "decon",
  "console" in the name), they're placed where the builder wanted them; the vessel hides the steel
  placeholder. Wrong mesh picked? Delete that actor.
- **Props** (folder AnomalyHunter/A07_Dressing): PPE shelves, crate and RSU panel in the airlock; ducts,
  floor grates and a biohazard sign along the spine; two workstations + a shelf in observation; crate
  cover, shelving, high ducts, a second damaged wall and biohazard signs in the chamber; two generators,
  racks and a crate in maintenance; ducts, grates and a crate in the tunnel; green exit signs over every
  door on the way out; an RSU panel outside the exit.
- **Collision:** Meshy meshes stay collision-free (far too dense), but every shelf / crate / desk /
  generator gets an invisible box `<label>_Collision`: you, bullets and the Gaunt stop at them, the
  NavMesh goes round them, and the chamber crates are actual cover.
- **Light fittings:** a hanging industrial lamp (`SM_FR_HangingLight`) over each A-07 light with the
  light moved just under it (the fitting doesn't cast shadows, so it can't trap its own light), plus
  6 new powered lights: link corridor, spine, two more in the chamber, two in the tunnel.
- **Sound of a working building** (C++, AFacilityMissionDirector):
  - each powered light buzzes quietly while it's lit (`SW_LightHum_Loop`, 7 m), and goes silent the
    instant the breach cuts the power while the tubes fade
  - actors tagged `AH_MachineHum` (the two A-07 generators and the original generator room's) spin up
    and rumble (`SW_Machine_Loop`, 25 m) when you restore power
  - director settings: Light Hum Volume (0.3), Machine Hum Volume (0.8); 0 turns either off
- **Room acoustics:** six Reverb Effect assets in `/Game/Generated/Reverb` and 15 Audio Volumes:

| Space | Reverb | Feel |
|---|---|---|
| original rooms, observation | RE_AH_Office (0.55 s) | dead, carpet-and-ceiling-tile |
| corridors, link, spine | RE_AH_Corridor (1.25 s) | hard flutter |
| airlock | RE_AH_SteelAirlock (1.0 s, bright) | metallic ring |
| generator room, maintenance | RE_AH_Plantroom (1.7 s) | concrete plant room |
| A-07 chamber | RE_AH_Chamber (4.6 s) | 9 m concrete hall - gunshots roll on |
| utility tunnel | RE_AH_Tunnel (2.4 s) | long echo down the tube |

  Every 3D sound in the game goes through AHRealism attenuation, which sends to reverb, so footsteps,
  doors, shots and the Gaunt all pick it up. If the Audio Volumes come out with no size (the script
  warns), place them by hand: Place Actors > Volumes > Audio Volume, scale over the room, and in
  Details > Reverb tick Apply Reverb and pick the RE_AH_ asset.
