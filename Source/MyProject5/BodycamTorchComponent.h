#pragma once

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "BodycamTorchComponent.generated.h"

class USoundBase;

/**
 * Chest-mounted torch. Attach it to the camera: it rides with the camera's position but
 * turns with a little lag, aims where you're looking (converging at AimDistance), shimmers
 * slightly, and runs on a battery that drains while on and recharges while off.
 * Below LowBatteryLevel it dims and starts to stutter; at empty it cuts out.
 */
UCLASS(ClassGroup = (Lights), meta = (BlueprintSpawnableComponent))
class MYPROJECT5_API UBodycamTorchComponent : public USpotLightComponent
{
    GENERATED_BODY()

public:
    UBodycamTorchComponent();

    void Toggle();
    void SetTorchOn(bool bOn, bool bPlayClick = true);

    bool  IsTorchOn() const   { return bTorchOn; }
    float GetBattery01() const { return Battery; }
    bool  IsBatteryLow() const { return Battery < LowBatteryLevel; }
    bool  CanAddBattery() const { return bUseBattery && Battery < 0.97f; }
    /** Swap in a found battery (adds Amount of a full charge). False if it's already full. */
    bool  AddBattery(float Amount);

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ---- Tweak live in PIE (select the pawn, Torch component). Cone angles, colour,
    //      attenuation and shadows are the normal spot light settings.

    UPROPERTY(EditAnywhere, Category = "Torch")
    bool bStartOn = true;

    /** Brightness when fully on (candelas). */
    UPROPERTY(EditAnywhere, Category = "Torch", meta = (ClampMin = "0"))
    float OnIntensity = 1600.f;

    /** Where the torch sits relative to the camera (X forward, Y right, Z up, cm). */
    UPROPERTY(EditAnywhere, Category = "Torch")
    FVector MountOffset = FVector(8.f, 12.f, -30.f);

    /** The beam aims at the point this far along the view, so it lines up with what you look at. */
    UPROPERTY(EditAnywhere, Category = "Torch", meta = (ClampMin = "50"))
    float AimDistance = 900.f;

    /** How quickly the beam catches up when you turn. Lower = heavier, more lag. */
    UPROPERTY(EditAnywhere, Category = "Torch", meta = (ClampMin = "1"))
    float FollowSpeed = 11.f;

    /** Slight constant brightness shimmer (fraction). */
    UPROPERTY(EditAnywhere, Category = "Torch", meta = (ClampMin = "0", ClampMax = "0.5"))
    float Shimmer = 0.03f;

    UPROPERTY(EditAnywhere, Category = "Torch|Battery")
    bool bUseBattery = true;

    /** Seconds of light from full. */
    UPROPERTY(EditAnywhere, Category = "Torch|Battery", meta = (ClampMin = "1"))
    float BatteryLifeSeconds = 420.f;

    /** Seconds to recharge from empty while switched off. */
    UPROPERTY(EditAnywhere, Category = "Torch|Battery", meta = (ClampMin = "1"))
    float RechargeSeconds = 150.f;

    /** Below this the beam dims and stutters. */
    UPROPERTY(EditAnywhere, Category = "Torch|Battery", meta = (ClampMin = "0", ClampMax = "1"))
    float LowBatteryLevel = 0.2f;

    /** Can't switch back on until it has recharged to this. */
    UPROPERTY(EditAnywhere, Category = "Torch|Battery", meta = (ClampMin = "0", ClampMax = "1"))
    float MinBatteryToSwitchOn = 0.05f;

    UPROPERTY(EditAnywhere, Category = "Torch|Audio")
    TSoftObjectPtr<USoundBase> ClickSound;

    UPROPERTY(EditAnywhere, Category = "Torch|Audio", meta = (ClampMin = "0"))
    float ClickVolume = 0.7f;

private:
    void UpdateAim(float DeltaTime, bool bSnap);
    float ComputeBrightness(float DeltaTime);

    bool  bTorchOn = false;
    float Battery = 1.f;
    float PowerAlpha = 0.f;       // 0..1 fade so switching isn't a hard pop
    float ShimmerTime = 0.f;
    float StutterTimer = 0.f;     // >0 while a low-battery dropout is happening
    float StutterLevel = 1.f;
    FQuat AimRotation = FQuat::Identity;
};
