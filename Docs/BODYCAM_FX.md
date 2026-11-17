# Bodycam look: lens + chest torch

Two components on `AMyFPSPawn`, both in C++:

| Component | What it does |
|---|---|
| `BodycamLens` (`UBodycamLensComponent`) | Sets the camera's own post-process: fisheye-style barrel distortion, colour fringing towards the edges, sensor grain, vignette, washed-out slightly cold colour, bloom, a little motion blur, slow exposure adaptation (walk from the dark into light and it whites out for a moment). |
| `Torch` (`UBodycamTorchComponent`) | Chest-mounted spot light. Rides with the camera, turns with a little lag, aims where you look (converging at 9 m), shimmers slightly, runs on a battery. **F** toggles it. |

Because it's all on the camera, it works in every map with no setup. It overrides the
same settings in any Post Process Volume (e.g. TestArena's), and leaves the rest alone.

## Setup (once)

1. Build the C++ with the editor **closed**.
2. In the editor: Tools > Execute Python Script > `Tools/build_bodycam_fx_ue58.py`.
   It makes `/Game/Generated/PostProcess/M_BodycamLens` (the distortion) and imports
   `SW_Torch_Click`. It touches nothing else and is safe to re-run.
   (`Tools/ue_setup_project.py` also runs it at the end.)
3. Press Play.

Without step 2 everything still works except the barrel distortion and the click sound
(Output Log says `BodycamLens: ... not built yet`).

## Tuning (select the pawn in PIE)

**BodycamLens**: `BarrelDistortion` (0.15-0.25 bodycam, 0.4+ peephole), `ChromaticAberration`,
`ChromaticAberrationStart`, `FilmGrain`, `Vignette`, `Saturation`, `Contrast`, `Tint`, `Bloom`,
`MotionBlur`, `ExposureSpeedUp` / `ExposureSpeedDown` (lower = longer white-out / longer to see
in the dark), `bEnableLens` to compare with the clean image.

**Torch**: `OnIntensity` (candelas), the normal spot light settings (cone angles, colour,
attenuation), `MountOffset`, `AimDistance`, `FollowSpeed` (lower = heavier lag), `Shimmer`,
`bStartOn`, battery: `bUseBattery`, `BatteryLifeSeconds` (7 min), `RechargeSeconds` (2.5 min,
recharges while off), `LowBatteryLevel` (below it the beam dims and stutters).

The HUD shows a thin amber battery bar under stamina while the torch is on or recharging;
it blinks red when low.

## Interference hook

`BodycamLens->SetInterference(0..1)` breaks the picture up: more fringing and grain, colour
drains out, random glitch bursts where the lens warps. Nothing calls it yet; it's meant for
anomalies getting close or the fear system, e.g. from an enemy's Tick:

```cpp
if (AMyFPSPawn* P = Cast<AMyFPSPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
{
    const float D = FVector::Dist(GetActorLocation(), P->GetActorLocation());
    P->GetBodycamLens()->SetInterference(1.f - FMath::Clamp((D - 300.f) / 1500.f, 0.f, 1.f));
}
```

## Notes

- The crosshair follows the lens warp (`DistortScreenPoint`), so it stays on what the gun
  points at even when sway moves it off centre.
- The distortion magnifies the centre by `1 + BarrelDistortion` and keeps the corners, so
  the view feels a little narrower than `BaseFOV`; that's how real bodycams look.
- The torch's beam only shows as a visible shaft once the map has volumetric fog
  (Exponential Height Fog with Volumetric Fog on). `VolumetricScatteringIntensity` is
  already set on the light.
- Untested in the engine when written (Nov 2026): the C++ and the material script were
  checked by reading them, and the distortion maths was simulated outside Unreal.
