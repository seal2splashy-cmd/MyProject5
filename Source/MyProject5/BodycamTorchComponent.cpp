#include "BodycamTorchComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UBodycamTorchComponent::UBodycamTorchComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // After the pawn has placed the camera for this frame.
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

    // It moves every frame, so it must be fully dynamic.
    Mobility = EComponentMobility::Movable;

    // Position follows the camera through the attachment; rotation is ours (lagged).
    SetUsingAbsoluteRotation(true);
    SetRelativeLocation(MountOffset);

    // A cold LED tactical light: tight hot centre, wider spill.
    IntensityUnits = ELightUnits::Candelas;
    Intensity = 0.f;
    LightColor = FColor(232, 240, 255);
    InnerConeAngle = 9.f;
    OuterConeAngle = 27.f;
    AttenuationRadius = 3200.f;
    SourceRadius = 1.5f;
    bUseInverseSquaredFalloff = true;
    CastShadows = true;
    VolumetricScatteringIntensity = 2.f;   // shows as a beam once the map has volumetric fog

    ClickSound = TSoftObjectPtr<USoundBase>(
        FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Torch_Click.SW_Torch_Click")));
}

void UBodycamTorchComponent::BeginPlay()
{
    Super::BeginPlay();

    SetRelativeLocation(MountOffset);
    UpdateAim(0.f, /*bSnap*/ true);
    SetTorchOn(bStartOn, /*bPlayClick*/ false);
    PowerAlpha = bTorchOn ? 1.f : 0.f;
}

void UBodycamTorchComponent::Toggle()
{
    SetTorchOn(!bTorchOn);
}

void UBodycamTorchComponent::SetTorchOn(bool bOn, bool bPlayClick)
{
    if (bOn && bUseBattery && Battery < MinBatteryToSwitchOn)
    {
        bOn = false;   // flat: the switch clicks but nothing happens
    }

    const bool bChanged = (bOn != bTorchOn);
    bTorchOn = bOn;

    if (bTorchOn)
    {
        SetVisibility(true);
    }

    if (bPlayClick)
    {
        if (USoundBase* Snd = ClickSound.LoadSynchronous())
        {
            UGameplayStatics::PlaySound2D(this, Snd, ClickVolume, bChanged ? 1.f : 0.85f);
        }
    }
}

void UBodycamTorchComponent::UpdateAim(float DeltaTime, bool bSnap)
{
    const USceneComponent* Cam = GetAttachParent();
    if (!Cam)
    {
        return;
    }

    const FVector CamLoc = Cam->GetComponentLocation();
    const FVector CamFwd = Cam->GetForwardVector();
    const FVector AimPoint = CamLoc + CamFwd * AimDistance;
    const FVector Dir = (AimPoint - GetComponentLocation()).GetSafeNormal(UE_SMALL_NUMBER, CamFwd);

    // Keep the camera's roll so the beam leans with the body when you tilt.
    const FQuat Target = FRotationMatrix::MakeFromXZ(Dir, Cam->GetUpVector()).ToQuat();

    AimRotation = bSnap ? Target : FMath::QInterpTo(AimRotation, Target, DeltaTime, FollowSpeed);
    SetWorldRotation(AimRotation);
}

float UBodycamTorchComponent::ComputeBrightness(float DeltaTime)
{
    // Battery
    if (bUseBattery)
    {
        if (bTorchOn)
        {
            Battery = FMath::Max(0.f, Battery - DeltaTime / BatteryLifeSeconds);
            if (Battery <= 0.f)
            {
                SetTorchOn(false, /*bPlayClick*/ false);   // died on its own
            }
        }
        else
        {
            Battery = FMath::Min(1.f, Battery + DeltaTime / RechargeSeconds);
        }
    }

    // Quick fade so switching on/off isn't a hard pop (about 60 ms).
    PowerAlpha = FMath::FInterpConstantTo(PowerAlpha, bTorchOn ? 1.f : 0.f, DeltaTime, 16.f);

    // Shimmer
    ShimmerTime += DeltaTime;
    float Level = 1.f + Shimmer * FMath::PerlinNoise1D(ShimmerTime * 7.f);

    // Low battery: dimmer, and random stuttering dropouts that get worse as it empties.
    if (bUseBattery && Battery < LowBatteryLevel && LowBatteryLevel > 0.f)
    {
        const float Low = 1.f - Battery / LowBatteryLevel;   // 0 at the threshold, 1 when empty
        Level *= FMath::Lerp(1.f, 0.35f, Low);

        if (StutterTimer > 0.f)
        {
            StutterTimer -= DeltaTime;
            Level *= StutterLevel;
        }
        else if (bTorchOn && FMath::FRand() < Low * 1.5f * DeltaTime)
        {
            StutterTimer = FMath::FRandRange(0.05f, 0.25f);
            StutterLevel = FMath::FRandRange(0.f, 0.4f);
        }
    }
    else
    {
        StutterTimer = 0.f;
    }

    return OnIntensity * Level * PowerAlpha;
}

void UBodycamTorchComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Lets MountOffset be tweaked live in PIE.
    if (!GetRelativeLocation().Equals(MountOffset, 0.01))
    {
        SetRelativeLocation(MountOffset);
    }

    UpdateAim(DeltaTime, /*bSnap*/ false);

    const float Brightness = ComputeBrightness(DeltaTime);
    SetIntensity(Brightness);

    // Fully off: hide it so it costs nothing (no shadow pass).
    if (!bTorchOn && PowerAlpha <= 0.f && IsVisible())
    {
        SetVisibility(false);
    }
}
