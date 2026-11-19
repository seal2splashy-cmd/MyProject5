# Realism pass (Nov 2026)

Four areas, each tunable from the Details panel. Everything degrades gracefully: missing sounds
are skipped, no NavMesh means straight-line movement, and without `AMyFPSPawn` the enemies fall
back to normal eyesight.

## 1. Enemy senses + behaviour (`AEnemy`, all enemies inherit it)

**Eyes**
- **Vision cone**: 60 degrees either side while patrolling or searching, 90 once hunting you. Within
  2.2 m it notices you whichever way it's facing.
- **Light matters**: it sees you up to 25 m when you're lit and only 7 m in the dark.
  `AMyFPSPawn::GetVisibility01` adds up every lamp that can actually see you (falloff with
  distance, shadows count), plus your torch glow. Crouching lowers it and sprinting raises it.
- **Your torch beam gives you away**: shine it at an enemy and it sees you from 30 m, whichever
  way it's facing.
- **Spotting takes time**: an awareness meter fills while it can see you. It's almost instant up
  close, and takes about 1.5 s at the edge of sight. At 40% a patrolling enemy stops and comes to
  check ("what was that?"); at 100% it chases. The meter drains slowly when you break line of sight.
- **Leaning works both ways**: the head ray goes to your camera, so peeking round a corner shows
  your head.

**Ears** (`AEnemy::ReportNoise`): every enemy in range hears it. Range is about halved through a
wall, and it can't pinpoint the sound exactly (about 10% of the distance off).

| Noise | Range |
|---|---|
| Gunshot | 45 m |
| Bullet hitting metal / concrete / wood / dirt | 22 / 15 / 12 / 7 m (where it lands, not where you are: it can draw them away) |
| Melee hit on a wall | 4-16 m by force |
| Sprinting / walking / crouching | 11 / 4.5 m / silent (x1.5 on metal, x0.5 on grass) |
| Landing from a jump/fall | up to 9 m |
| Heavy breathing when exhausted | up to 3.5 m |
| Reload / dry fire / torch click | 3.5 / 2.5 / 1.5 m |
| Door open / close / slam / locked rattle | 4.5 / 6.5 / 16 / 3.5 m (Docs/DOORS_PICKUPS.md) |
| Getting hurt (a grunt) | 6 m |

**Search state** (new, between Chase and Patrol): when it hears something, half-sees something or
loses you, it goes to the spot, stops and looks around, checks a couple of nearby places for 14 s,
then goes back to patrolling. A louder noise re-targets it.

**Its own footsteps**: you hear enemies walking, muffled through walls. The Gaunt's steps are
heavier and lower, it sees better in the dark (11 m) and hears 30% better.

## 2. Gun handling (`AGun`, `ABullet`)
- **Real magazines**: a part-used mag goes back in the pouch with its rounds, and R loads your
  fullest one. HUD: `rounds ready | in spare mags (N mags)`.
- **Chamber**: 12+1 with a round chambered. Firing the last one locks the slide back; a reload
  from empty takes 2.2 s (releasing the slide) against 1.6 s for a tactical reload.
- **Accuracy cone**: 1.4 degrees from the hip, 0.3 aimed, plus up to 2.2 moving, 4 in the air and
  1 out of breath. Crouched x0.8. The crosshair opens to match and fades out as the sights come up.
- **Recoil** flips the muzzle **up** (it used to kick the view down). Quick follow-up shots stack
  up to x1.8 and drift slightly right, a quarter of the climb stays, and aimed or crouched is steadier.
- **ADS** brings the gun up to the eye line, and the arms follow. You walk at 60% speed and can't sprint.
- **Breathing sway**: the sights rise and fall with each breath, bigger and faster when you're tired.
- **Muzzle flash** lights the room for about 45 ms.
- **Bullets** (360 m/s):
  - go through up to 25 cm of wood, sheet metal, glass and leaves, losing energy
  - glance off concrete (under 12 degrees) and metal (under 18 degrees) with a ricochet whine
  - do x2.5 damage to the head and x0.7 to the legs, and spent bullets hurt less
  - play the hit sound for whatever they land on

## 3. Player movement + body (`AMyFPSPawn`)
- **Speeds**: walk 3 m/s, sprint 6.5 m/s, crouch 1.65 m/s (it was 9 and 18 m/s before; the first
  realism pass went down to 2.3 and 5.2, which felt too slow). Enemies chase at 4-4.5 m/s, so
  sprinting outruns them, until your stamina runs out after about 5 s.
- **Momentum**: you speed up and slow down over a few tenths of a second, keep your momentum in
  the air, and lose the speed going into a wall.
- **Footstep-paced head bob**: one bob per step, with steps paced by the distance you actually
  walk (110 cm walking, 165 cm sprinting), so cadence follows speed. Your weight shifts side to side.
- **Per-surface footsteps** on concrete, metal, wood, dirt, grass and foliage.
- **Fall damage**: drops over about 4 m hurt and about 15 m kills. A hard landing also knocks the
  wind out of you.
- **Breathing**: heavier the more tired or hurt you are, pitch rising with it.
- **Lean (Z/C) stops at walls**, so you can't see through them. You can't lean while sprinting.
- **Less cartoon camera**: turn roll is 3 degrees (was 25), strafe roll 1.5 (was 15), and sprinting
  widens the FOV by 3 (was 20). A chest camera doesn't zoom.
- Fixed a drift where the camera's sideways jitter accumulated every frame.

## Fear (`AMyFPSPawn::UpdateFear`)
Fear builds fast and fades slowly. It comes from:
- an enemy hunting you nearby (the worst), searching nearby, or just wandering right next to you
- the Gaunt's presence (the bodycam interference)
- standing in the dark
- being badly hurt

What it does:
- **Heartbeat**: fades in from about 15% fear, and speeds up from ~66 to ~105 bpm.
- **Hands shake**: the aim wobbles (aiming only partly steadies it) and spread widens.
- **Breathing** quickens.
- **The picture narrows**: more vignette, a bit grainier and greyer.

Tune it under Fear on the pawn: FearEnemyRange, FearRiseRate / FearFallRate, FearAimWobbleDeg,
HeartbeatVolume.

## 4. Audio + bodycam look
- **Realistic 3D sound** (`AHRealism::PlayAt`): sounds get quieter and duller with distance as air
  soaks up the highs, and come through muffled and quieter when there's a wall in between. Used for
  impacts, melee and enemy footsteps.
- **Surface impact sounds** for bullets and melee. Only metal clangs now; wood knocks and concrete
  thuds.
- **Bodycam mic auto-gain**: a gunshot ducks the background hiss, which creeps back over a second or so.
- **Gunshot flash**: the sensor whites out and flares for a couple of frames, with a little tearing.
- **Turn blur**: fast whip-turns smear like a real low-shutter body camera.
- **Burn-in overlay**, top right: blinking REC, date and time, and the unit number (`AH-BC 0417`,
  editable on the HUD).

## New sounds
`python Tools/generate_assets.py` (or only the new ones: `build_realism_audio()`):
`SW_Footstep_Metal/Wood/Dirt/Grass_*`, `SW_Impact_Concrete/Metal/Wood/Dirt/Glass/Flesh_*`,
`SW_Ricochet_*`, `SW_Breath_Heavy_Loop`, `SW_Heartbeat_Loop`. They're already generated in `Assets/Audio`; run
`Tools/ue_setup_project.py` to import them (every `*_Loop` is set to loop).

## Debugging (console, ` key)
- `ah.DebugSenses 1`: each enemy's vision cone (cyan patrol, orange search, red hunting), its
  2.2 m close-sense ring, state and awareness %, and the search spot.
- `ah.DebugNoise 1`: every noise as a yellow sphere of how far it carries.
- Enemy > Pathfinding > **Draw Debug Path**: its route.

## Test plan (NEA evidence)

| # | Test | Expected |
|---|---|---|
| S1 | Walk up behind a patrolling enemy, crouched, torch off | it doesn't turn round until you're within 2.2 m |
| S2 | Same, standing in a lit corridor | awareness climbs while you're in its cone; it turns to look at 40% |
| S3 | Torch off (T) in a dark room at 10 m, then switch it on facing it | not seen, then seen at once |
| S4 | Fire a shot in another room | enemies within ~22 m (through walls) come and search near the shot |
| S5 | Shoot a wall far from you | enemies near the impact go there, not to you |
| S6 | Sprint past an enemy 8 m away behind a wall | it hears you (debug sphere reaches it) and comes to look |
| S7 | Break line of sight mid-chase | it goes to where it last saw you, looks around for ~14 s, then patrols |
| G1 | Empty the magazine | 13 shots, slide locks; empty reload takes visibly longer |
| G2 | Reload with 5 left | 12+1 after; the 5-round mag is in the pouch (reserve count) |
| G3 | Hip-fire vs aim at a wall at 10 m | hip group ~0.5 m wide, aimed group ~10 cm |
| G4 | Shoot through a wooden crate / tree canopy | bullet continues and hits what's behind |
| G5 | Shoot concrete at a very shallow angle | ricochet whine, bullet goes off at an angle |
| G6 | Headshot vs leg shot on the Gaunt | head kills in 2, legs need 6 |
| M1 | Walk from concrete onto grass / a metal grate | footstep sound changes |
| M2 | Drop from ~5 m | some damage; from 15 m: death |
| M3 | Sprint until empty | breathing builds, then settles over ~10 s |
| M4 | Lean into a wall | camera stops at the wall |
| F1 | Let an enemy chase you, then hide | heartbeat fades in fast, aim shakes; ~10 s after it loses you the heart slows and fades |
