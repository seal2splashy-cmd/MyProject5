# Horror systems: bodycam glitches, the Gaunt's hunt, the building's sound

## After pulling

1. Build the C++.
2. In the editor (any map): `Tools/build_bodycam_fx_ue58.py` - rebuilds M_BodycamLens with the glitch
   parameters (without it the old material still works, just without the new glitches).
3. `Tools/import_audio_ue58.py` - imports the 31 new sounds (Gaunt voice, ambience, stingers, recorder
   crackle) and sets the loops. Only imports what's new; doesn't touch maps.
4. A-07 map open: `Tools/place_story_ue58.py` again - tags the dead operators so seeing them stings
   (it skips everything else that's already there). Ctrl+S.

## Bodycam glitches (UBodycamLensComponent + M_BodycamLens)

| Effect | When | How |
|---|---|---|
| Rolling shutter | always, on fast turns | the sensor reads rows top to bottom: a fast pan shears the picture (`RollingShutter` 0.07), a fast tilt squashes it (`RollingShutterSquash`) |
| Macroblock corruption | glitch bursts | some 32x18 blocks fetch from the wrong place |
| Tear line | glitch bursts | a band of rows slips sideways |
| Dropped frames | strong bursts (`FrameDropChance`) | the picture freezes 80-260 ms then jumps; the HUD timestamp freezes too, the REC dot stops; a codec blip |
| Audio crackle | bursts over 0.35 | bit-crushed crackle on the audio track |
| NO SIGNAL | signal loss > 0.55 | REC flickers to a red NO SIGNAL |

Bursts come from the Gaunt's interference (distance + hunting), getting hit (any enemy hit glitches
by damage, and shoves you back - `HitKnockback`), the grab, stingers and gunshots.
`ForceGlitch(Strength, bAllowFrameDrop)` for anything else. The settings menu's BODYCAM EFFECTS scales
all of it (full / reduced / off).

## The Gaunt hunts (AGauntAnomaly, Details > Gaunt|Hunting / Gaunt|Voice)

- **Stalking** - further than `StalkDistance` (11 m) it only moves while you're not looking at it
  (within `LookedAtCosine`, ~28 degrees, with line of sight). Look at it: it freezes and clicks at you.
  Look away: it closes in at `StalkSpeedScale` (1.35x). Inside 11 m it's the Q-learning chase as before.
- **Light** - the torch beam on its head within `TorchFlinchDistance` (14 m) makes it shriek, recoil
  and back off for `FlinchDuration` (2 s); then the light won't stop it for `FlinchCooldown` (6 s).
- **Grab** - a hit that would kill you becomes a `GrabDuration` (1.8 s) grab: no moving or looking,
  your view is dragged to its face, it screams, the bodycam falls apart... then the blow lands. You can
  still fire during it.
- **Voice** - wet breathing loop on its body (louder and faster when hunting, muffled by walls), click
  trains (quicker while you stare at it), a growl when it first spots you, a shriek when it dies.
- **Catwalks** - it paths over the NavMesh, which covers the catwalks and stairs: it follows you up.

## The building (UAHAmbienceComponent on the pawn, Details > Ambience)

- Drone bed (`SW_Amb_Drone_Loop`): `DroneVolume` 0.3 calm, up to +`DroneFearBoost` with fear and after
  the breach, sinking in pitch.
- Distant sounds from real positions 12-35 m away (walls muffle them, the reverb rooms colour them):
  metal groans, pipe knocks, settling creaks, heavy thuds, debris trickling. Every 16-38 s calm, down to
  30% of that at full fear / during the alarm.
- Stingers: the first time a tagged `AH_Stinger` thing (the dead operators) is in your view, in line of
  sight, within 11 m - a hit + dissonant cluster, and a small bodycam twitch. Once each, 12 s apart.

## Sounds (Tools/generate_assets.py, build_horror_audio, seed 2613)

`SW_Gaunt_Breath_Loop`, `SW_Gaunt_Click_01-03`, `SW_Gaunt_Growl_01`, `SW_Gaunt_Screech_01`,
`SW_Gaunt_Scream_01`, `SW_Amb_MetalGroan_01-03`, `SW_Amb_PipeKnock_01-03`, `SW_Amb_Creak_01-02`,
`SW_Amb_Thud_01-02`, `SW_Amb_Debris_01-02`, `SW_Amb_Drone_Loop`, `SW_Stinger_01-02`,
`SW_Bodycam_Crackle_01-03`, `SW_Bodycam_Dropout_01`. Missing ones are skipped silently.

## Playtest

- [ ] Whip the mouse left/right: verticals lean (rolling shutter); flick up/down: the picture squashes
- [ ] Gaunt at 15 m, stare at it: it stops and clicks; look away 2 s and back: it's closer
- [ ] Torch in its face at 8 m: shriek, it backs off; again straight away: it doesn't care
- [ ] Get to low health and let it reach you: view dragged to its face, scream, NO SIGNAL, death
- [ ] Hit by it at full health: shoved back, glitch burst, maybe a frozen frame
- [ ] Stand still in the dark: every 20-30 s something groans / knocks somewhere; faster when scared
- [ ] Walk into storage: a sting when you first see the body
