#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BodycamLensComponent.generated.h"

class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Makes the player camera look like body-worn camera footage:
 *  - fisheye-style barrel distortion (post-process material M_BodycamLens, made by
 *    Tools/build_bodycam_fx_ue58.py; skipped if it hasn't been built)
 *  - colour fringing towards the edges, sensor grain, vignette
 *  - flat, slightly cold colour, blooming highlights, a little motion blur
 *  - slow exposure adaptation, so walking from the dark into light briefly whites out
 *
 * Everything is set on the camera's own post-process settings, so it works in every map.
 * These override the same settings in any Post Process Volume (e.g. the one in TestArena).
 *
 * SetInterference(0..1) makes the picture break up: more fringing and grain, colour
 * draining out, random glitch bursts where the lens warps. Nothing calls it yet; it's
 * there for anomalies/fear to drive later.
 */
UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class MYPROJECT5_API UBodycamLensComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBodycamLensComponent();

    /** Call once from the owner's BeginPlay with the camera to treat. */
    void InitLens(UCameraComponent* InCamera);

    /** 0 = clean bodycam, 1 = heavy interference. Smoothed internally. */
    UFUNCTION(BlueprintCallable, Category = "Bodycam")
    void SetInterference(float Amount01);

    /**
     * For things that cause interference every frame (e.g. each nearby anomaly): call it in Tick.
     * The strongest report this frame wins, so several sources don't fight; when they stop
     * reporting the picture clears up again on its own.
     */
    void ReportInterference(float Amount01);

    float GetInterference() const { return Interference; }

    /** How fast the camera is turning (degrees/s). Fast turns smear like a real low-shutter bodycam. */
    void SetCameraMotion(float AngularSpeedDegPerSec);

    /** A shot was fired: the sensor whites out for a moment and flares, then recovers. */
    void OnGunshot();

    /** 0..1 fear from the pawn: the picture narrows (vignette), drains a little and gets grainier. */
    void SetStress(float Stress01) { Stress = FMath::Clamp(Stress01, 0.f, 1.f); }

    /**
     * Where a point from ProjectWorldLocationToScreen actually shows up once the lens has
     * warped the picture. Use it for anything the HUD draws over the world (the crosshair).
     */
    FVector2D DistortScreenPoint(const FVector2D& ScreenPos, const FVector2D& ViewportSize) const;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ---- Lens. Tweak live in PIE (select the pawn, BodycamLens component).

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens")
    bool bEnableLens = true;

    /** Fisheye bulge. 0 = flat, 0.15-0.25 = typical bodycam, 0.4+ = peephole. */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "0.6"))
    float BarrelDistortion = 0.18f;

    /** Colour fringing (UE "Chromatic Aberration" intensity). */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "5"))
    float ChromaticAberration = 1.4f;

    /** How far from the centre fringing starts (0 = everywhere, 1 = only the corners). */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "1"))
    float ChromaticAberrationStart = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "1"))
    float FilmGrain = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "1"))
    float Vignette = 0.7f;

    /** 1 = normal colour. Bodycams are a bit washed out. */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "2"))
    float Saturation = 0.8f;

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "2"))
    float Contrast = 1.08f;

    /** Slight cold/green cast of a cheap sensor. White = none. */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens")
    FLinearColor Tint = FLinearColor(0.96f, 1.0f, 0.97f);

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "8"))
    float Bloom = 1.3f;

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "1"))
    float MotionBlur = 0.35f;

    /** Extra motion blur at a fast whip-turn (FastTurnSpeed degrees/s or more). */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "1"))
    float TurnMotionBlur = 0.45f;

    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "30"))
    float FastTurnSpeed = 360.f;

    /** How bright the picture flashes on a gunshot (extra colour gain at the peak). */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0", ClampMax = "4"))
    float GunshotFlash = 1.2f;

    /** Exposure adaptation speed, dark -> bright. Lower = longer white-out. (UE default 3) */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0.02", ClampMax = "20"))
    float ExposureSpeedUp = 0.6f;

    /** Exposure adaptation speed, bright -> dark. Lower = eyes take longer in the dark. (UE default 1) */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens", meta = (ClampMin = "0.02", ClampMax = "20"))
    float ExposureSpeedDown = 0.8f;

    /** Post-process material doing the barrel distortion. Scalar parameter "Distortion". */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Lens")
    TSoftObjectPtr<UMaterialInterface> LensMaterial;

    // ---- Interference

    /** How fast the picture follows SetInterference (per second). */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Interference", meta = (ClampMin = "0.1"))
    float InterferenceResponse = 4.f;

    /** Glitch bursts per second at full interference. */
    UPROPERTY(EditAnywhere, Category = "Bodycam|Interference", meta = (ClampMin = "0"))
    float GlitchRate = 3.f;

private:
    void ApplySettings();

    UPROPERTY(Transient)
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> LensMID;

    float Interference = 0.f;
    float InterferenceTarget = 0.f;
    float ReportedThisFrame = 0.f;   // max of ReportInterference calls since the last tick
    float Glitch = 0.f;          // 1 at the start of a burst, decays fast
    float GlitchPhase = 0.f;
    float CurrentDistortion = 0.f;   // what the material is using this frame
    float TurnBlur = 0.f;            // 0..1, smoothed
    float Flash = 0.f;               // 1 at a gunshot, decays in ~0.15 s
    float Stress = 0.f;              // fear, set by the pawn
};
