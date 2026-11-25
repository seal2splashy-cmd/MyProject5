#include "BodycamLensComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RealismUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UBodycamLensComponent::UBodycamLensComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Read the final camera state for the frame.
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

    LensMaterial = TSoftObjectPtr<UMaterialInterface>(
        FSoftObjectPath(TEXT("/Game/Generated/PostProcess/M_BodycamLens.M_BodycamLens")));
}

void UBodycamLensComponent::InitLens(UCameraComponent* InCamera)
{
    Camera = InCamera;
    if (!Camera)
    {
        return;
    }

    Camera->PostProcessBlendWeight = 1.f;

    if (UMaterialInterface* Mat = LensMaterial.LoadSynchronous())
    {
        LensMID = UMaterialInstanceDynamic::Create(Mat, this);
        if (LensMID)
        {
            Camera->PostProcessSettings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, LensMID));
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("BodycamLens: %s not built yet, no barrel distortion (run Tools/build_bodycam_fx_ue58.py)"),
            *LensMaterial.ToString());
    }

    ApplySettings();
}

void UBodycamLensComponent::SetInterference(float Amount01)
{
    InterferenceTarget = FMath::Clamp(Amount01, 0.f, 1.f);
}

void UBodycamLensComponent::ReportInterference(float Amount01)
{
    ReportedThisFrame = FMath::Max(ReportedThisFrame, FMath::Clamp(Amount01, 0.f, 1.f));
}

void UBodycamLensComponent::SetCameraMotion(float AngularSpeedDegPerSec)
{
    const float Target = FMath::Clamp(AngularSpeedDegPerSec / FMath::Max(FastTurnSpeed, 1.f), 0.f, 1.f);
    const UWorld* W = GetWorld();
    const float Dt = W ? W->GetDeltaSeconds() : 0.016f;
    TurnBlur = FMath::FInterpTo(TurnBlur, Target, Dt, Target > TurnBlur ? 14.f : 5.f);
}

void UBodycamLensComponent::OnGunshot()
{
    Flash = 1.f;
    Glitch = FMath::Max(Glitch, 0.25f);   // a little sensor tearing from the shock
    GlitchPhase = FMath::FRandRange(0.f, 2.f * UE_PI);
}

void UBodycamLensComponent::ForceGlitch(float Strength01, bool bAllowFrameDrop)
{
    StartGlitch(FMath::Clamp(Strength01, 0.f, 1.f), bAllowFrameDrop);
}

void UBodycamLensComponent::StartGlitch(float Strength, bool bAllowFrameDrop)
{
    if (EffectsScale <= 0.01f || Strength <= 0.f)
    {
        return;
    }
    Glitch = FMath::Max(Glitch, Strength);
    GlitchPhase = FMath::FRandRange(0.f, 2.f * UE_PI);
    Seed = FMath::FRandRange(0.f, 1000.f);
    Blocks = FMath::Max(Blocks, Strength * FMath::FRandRange(0.04f, 0.22f));
    if (FMath::FRand() < 0.6f)
    {
        Tear = FMath::FRandRange(-0.05f, 0.05f) * Strength;
        TearY = FMath::FRand();
    }

    // the recorder chokes: drop a few frames
    if (bAllowFrameDrop && Camera && HoldTimeLeft <= 0.f && FMath::FRand() < FrameDropChance * Strength * EffectsScale)
    {
        HoldTimeLeft = FMath::FRandRange(FrameDropDuration.X, FrameDropDuration.Y);
        HeldTransform = Camera->GetComponentTransform();
        if (USoundBase* Drop = AHRealism::GeneratedSound(TEXT("SW_Bodycam_Dropout_01")))
        {
            UGameplayStatics::PlaySound2D(this, Drop, 0.6f * Strength);
        }
    }
    // the audio track crackles with it
    if (Strength > 0.35f)
    {
        if (USoundBase* Crackle = AHRealism::RandomVariant(TEXT("SW_Bodycam_Crackle"), 3))
        {
            UGameplayStatics::PlaySound2D(this, Crackle, 0.25f + 0.5f * Strength, FMath::FRandRange(0.9f, 1.1f));
        }
    }
}

void UBodycamLensComponent::UpdateRollingShutter(float DeltaTime)
{
    // Signed turn rates from the camera itself (the pawn has already placed it this frame)
    const FRotator Rot = Camera->GetComponentRotation();
    if (bHaveLastRot && DeltaTime > 0.f)
    {
        const float Yaw = static_cast<float>(FRotator::NormalizeAxis(Rot.Yaw - LastCameraRot.Yaw)) / DeltaTime;
        const float Pitch = static_cast<float>(FRotator::NormalizeAxis(Rot.Pitch - LastCameraRot.Pitch)) / DeltaTime;
        YawRate = FMath::FInterpTo(YawRate, Yaw, DeltaTime, 18.f);
        PitchRate = FMath::FInterpTo(PitchRate, Pitch, DeltaTime, 18.f);
    }
    LastCameraRot = Rot;
    bHaveLastRot = true;
}

void UBodycamLensComponent::UpdateFrameHold(float DeltaTime)
{
    if (HoldTimeLeft <= 0.f)
    {
        return;
    }
    HoldTimeLeft -= DeltaTime;
    // Keep showing the old frame: the camera stays where it was (the pawn puts it back next tick)
    Camera->SetWorldTransform(HeldTransform);
}

void UBodycamLensComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!Camera)
    {
        return;
    }

    // Sources report in their own Tick, which runs before ours (we tick in PostUpdateWork).
    const float Target = FMath::Max(InterferenceTarget, ReportedThisFrame);
    ReportedThisFrame = 0.f;
    Interference = FMath::FInterpTo(Interference, Target, DeltaTime, InterferenceResponse);

    UpdateRollingShutter(DeltaTime);

    // Random glitch bursts, more often the stronger the interference.
    if (Interference > 0.01f && FMath::FRand() < Interference * GlitchRate * DeltaTime * EffectsScale)
    {
        StartGlitch(FMath::FRandRange(0.6f, 1.f) * FMath::Max(Interference, 0.4f), Interference > 0.35f);
    }
    Glitch = FMath::FInterpTo(Glitch, 0.f, DeltaTime, 9.f);
    Blocks = FMath::FInterpTo(Blocks, 0.f, DeltaTime, 12.f);
    Tear = FMath::FInterpTo(Tear, 0.f, DeltaTime, 10.f);
    UpdateFrameHold(DeltaTime);
    GlitchPhase += DeltaTime * 45.f;
    Flash = FMath::FInterpTo(Flash, 0.f, DeltaTime, 18.f);

    ApplySettings();
}

void UBodycamLensComponent::ApplySettings()
{
    FPostProcessSettings& S = Camera->PostProcessSettings;

    // Turning the lens off hands every setting back to the map's post-process volume / defaults.
    const bool bOn = bEnableLens;
    S.bOverride_SceneFringeIntensity = bOn;
    S.bOverride_ChromaticAberrationStartOffset = bOn;
    S.bOverride_FilmGrainIntensity = bOn;
    S.bOverride_VignetteIntensity = bOn;
    S.bOverride_ColorSaturation = bOn;
    S.bOverride_ColorContrast = bOn;
    S.bOverride_SceneColorTint = bOn;
    S.bOverride_BloomIntensity = bOn;
    S.bOverride_MotionBlurAmount = bOn;
    S.bOverride_AutoExposureSpeedUp = bOn;
    S.bOverride_AutoExposureSpeedDown = bOn;
    S.bOverride_ColorGain = bOn;
    S.bOverride_AutoExposureBias = bOn;

    if (!bOn)
    {
        CurrentDistortion = 0.f;
        if (LensMID)
        {
            LensMID->SetScalarParameterValue(TEXT("Distortion"), 0.f);
        }
        return;
    }

    const float I = Interference * EffectsScale;
    const float G = Glitch * EffectsScale;

    S.SceneFringeIntensity = ChromaticAberration * (1.f + 2.f * I) + 4.f * G;
    S.ChromaticAberrationStartOffset = FMath::Lerp(ChromaticAberrationStart, 0.f, FMath::Max(I, G));
    S.FilmGrainIntensity = FMath::Clamp(FilmGrain + 0.45f * I + 0.5f * G + 0.12f * Stress, 0.f, 1.f);
    S.VignetteIntensity = Vignette + 0.25f * I + 0.4f * Stress;   // fear narrows the view

    const float Sat = Saturation * (1.f - 0.55f * I) * (1.f - 0.4f * G) * (1.f - 0.25f * Stress);
    S.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
    S.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.f);
    S.SceneColorTint = Tint;
    S.BloomIntensity = Bloom * (1.f + 2.f * Flash);
    S.MotionBlurAmount = bMotionBlurOn ? FMath::Clamp(MotionBlur + TurnMotionBlur * TurnBlur, 0.f, 1.f) : 0.f;
    S.AutoExposureBias = BrightnessEV;
    // gunshot: the cheap sensor whites out for a couple of frames
    const float Gain = 1.f + GunshotFlash * Flash;
    S.ColorGain = FVector4(Gain, Gain * 0.97f, Gain * 0.9f, 1.f);
    S.AutoExposureSpeedUp = ExposureSpeedUp;
    S.AutoExposureSpeedDown = ExposureSpeedDown;

    if (LensMID)
    {
        // During a glitch the lens "breathes": distortion wobbles quickly about its normal value.
        CurrentDistortion = FMath::Max(BarrelDistortion + 0.12f * G * FMath::Sin(GlitchPhase) + 0.04f * I, 0.f);
        LensMID->SetScalarParameterValue(TEXT("Distortion"), CurrentDistortion);

        // Rolling shutter: pan right -> the bottom rows were read later, further right
        const float Fast = FMath::Max(FastTurnSpeed, 1.f);
        LensMID->SetScalarParameterValue(TEXT("Skew"),
            FMath::Clamp(YawRate / Fast, -1.5f, 1.5f) * RollingShutter * EffectsScale);
        LensMID->SetScalarParameterValue(TEXT("Squash"),
            FMath::Clamp(-PitchRate / Fast, -1.5f, 1.5f) * RollingShutterSquash * EffectsScale);
        LensMID->SetScalarParameterValue(TEXT("Blocks"), Blocks * EffectsScale);
        LensMID->SetScalarParameterValue(TEXT("Seed"), Seed);
        LensMID->SetScalarParameterValue(TEXT("Tear"), Tear * EffectsScale);
        LensMID->SetScalarParameterValue(TEXT("TearY"), TearY);
    }
    else
    {
        CurrentDistortion = 0.f;
    }
}

FVector2D UBodycamLensComponent::DistortScreenPoint(const FVector2D& ScreenPos, const FVector2D& ViewportSize) const
{
    const double K = CurrentDistortion;
    if (K <= 0.0 || ViewportSize.X < 1.0 || ViewportSize.Y < 1.0)
    {
        return ScreenPos;
    }

    // The material maps output UV p to source UV s = 0.5 + (p - 0.5) * (1 + K r2(p)) / (1 + K)
    // (see Tools/build_bodycam_fx_ue58.py). Here we have s and want p: a few fixed-point
    // steps converge for any sane K.
    const double Aspect = ViewportSize.X / ViewportSize.Y;
    const double Norm = 0.25 * (Aspect * Aspect + 1.0);
    const FVector2D S = ScreenPos / ViewportSize - FVector2D(0.5, 0.5);

    FVector2D P = S;
    for (int32 Step = 0; Step < 6; ++Step)
    {
        const double R2 = (FMath::Square(P.X * Aspect) + FMath::Square(P.Y)) / Norm;
        P = S * ((1.0 + K) / (1.0 + K * R2));
    }
    return (P + FVector2D(0.5, 0.5)) * ViewportSize;
}
