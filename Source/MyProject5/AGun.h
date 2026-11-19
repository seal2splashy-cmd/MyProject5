#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGun.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class ABullet;
class UCameraComponent;
class USoundBase;

/**
 * The pistol. Handles like a real one (Docs/REALISM.md):
 *  - real magazines: a part-used mag goes back in the pouch with its rounds, you reload the
 *    fullest one; a round in the chamber makes it 12+1, and a reload from empty is slower
 *    because the slide has to be released
 *  - accuracy depends on what you're doing: aimed / hip, standing / moving / in the air, out of breath
 *  - recoil flips the muzzle up and stacks on quick follow-up shots, drifting slightly right
 *  - the muzzle flash lights the room for a few hundredths of a second
 *  - aiming brings the sights up to your eye (the arms follow)
 *  - gunshots, dry fire and reloads are noises enemies hear (AEnemy::ReportNoise)
 */
UCLASS()
class MYPROJECT5_API AGun : public AActor
{
    GENERATED_BODY()

public:
    AGun();

    bool Fire(const FVector& EyeLocation, const FRotator& EyeRotation, AController* InstigatorController, APawn* OwningPawn);
    void StartReload();
    bool IsReloading() const { return bReloading; }

    /** The pawn tells the gun what it's doing every tick (affects spread, recoil and ADS pose). */
    void SetHandling(float InAimAlpha, float InMove01, bool bInAir, float InFatigue01, bool bInCrouched);

    float GetRecoilPitch() const { return RecoilPitch; }
    float GetRecoilYaw()   const { return RecoilYaw; }
    /** Recoil of the shot just fired (degrees, pitch up / yaw right), after stacking and stance. */
    float GetLastShotRecoilPitch() const { return LastShotRecoilPitch; }
    float GetLastShotRecoilYaw()   const { return LastShotRecoilYaw; }
    float GetWeaponFOVKick() const { return WeaponFOVKick; }
    /** Current accuracy cone half-angle in degrees (the HUD opens the crosshair to match). */
    float GetCurrentSpreadDeg() const { return CurrentSpreadDeg; }
    float GetAimAlpha() const { return AimAlpha; }

    /** Rounds ready to fire: the magazine plus the one in the chamber. */
    int32 GetAmmoInMag() const { return AmmoInMag + (bRoundChambered ? 1 : 0); }
    /** Rounds in the spare magazines. */
    int32 GetReserveAmmo() const;
    int32 GetSpareMagCount() const { return SpareMags.Num(); }
    bool CanAddSpareMag() const { return SpareMags.Num() < MaxSpareMags; }
    /** A magazine found in the world goes in the pouch. False if the pouch is full. */
    bool AddSpareMag(int32 Rounds);

    /** World time of the last trigger pull (shot or dry fire). Used for the trigger-finger squeeze. */
    float GetLastFireTime() const { return LastFireTime; }

    void EquipToCamera(UCameraComponent* Camera, const FTransform& RelativeTransform = FTransform::Identity);
    void ApplyFirstPersonOffset(const FVector& CameraLocalPosOffset, const FRotator& CameraLocalRotOffset);

    // Returns a world-space point along the gun's aim direction (camera forward + sway).
    // Used by the crosshair AND by Fire(), so both share one source of truth.
    FVector GetMuzzleAimPoint(float Distance = 10000.f) const;

    /** Mesh the first-person arms attach to, so they inherit every bit of sway. */
    UStaticMeshComponent* GetGunMesh() const { return MeshComp; }

    /** Hide/show the gun (used when swapping to melee). */
    void SetGunHidden(bool bShouldHide);

    /**
     * Where the operator arms' hands should grip, world space. Follows the gun's sway.
     * OutAxis runs up the grip (towards the slide). Returns false while the gun is holstered.
     */
    bool GetHandTargets(FVector& OutRightGrip, FVector& OutLeftGrip, FVector& OutAxis) const;

protected:
    virtual void BeginPlay() override;

private:
    bool GetMuzzleTransform(FVector& OutLoc, FRotator& OutRot) const;
    void FinishReload();
    void UpdateMuzzleFlash(float DeltaTime);

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* MeshComp;

    /** Lights the surroundings for a moment on every shot. */
    UPROPERTY(VisibleAnywhere)
    UPointLightComponent* MuzzleLight;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float Damage = 28.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    int32 MagSize = 12;

    /** Rounds in the magazine in the gun (not counting the chamber). */
    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    int32 AmmoInMag = 12;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    bool bRoundChambered = true;

    /** Spare magazines in the pouch and how many rounds each holds. */
    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    TArray<int32> SpareMags = { 12, 12, 12 };

    /** How many spare magazines the pouch holds. */
    UPROPERTY(EditAnywhere, Category = "Gun|Stats", meta = (ClampMin = 1))
    int32 MaxSpareMags = 6;

    /** Semi-auto: how fast you can realistically pull the trigger. */
    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float FireRate = 4.f;

    /** Reload with a round still chambered (just swap the mag). */
    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float ReloadTime = 1.6f;

    /** Reload from empty: swap the mag and release the slide. */
    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float EmptyReloadTime = 2.2f;

    /** 9 mm from a pistol barrel: ~360 m/s. */
    UPROPERTY(EditAnywhere, Category = "Gun|Projectile")
    float MuzzleVelocity = 36000.f;

    // ---- Accuracy (cone half-angle, degrees) ----
    UPROPERTY(EditAnywhere, Category = "Gun|Accuracy")
    float HipSpreadDeg = 1.4f;

    UPROPERTY(EditAnywhere, Category = "Gun|Accuracy")
    float AimSpreadDeg = 0.3f;

    /** Extra at a full sprint (scales with speed). */
    UPROPERTY(EditAnywhere, Category = "Gun|Accuracy")
    float MoveSpreadDeg = 2.2f;

    UPROPERTY(EditAnywhere, Category = "Gun|Accuracy")
    float AirSpreadDeg = 4.f;

    /** Extra when completely out of breath. */
    UPROPERTY(EditAnywhere, Category = "Gun|Accuracy")
    float FatigueSpreadDeg = 1.f;

    /** Each quick follow-up shot adds this before the gun settles. */
    UPROPERTY(EditAnywhere, Category = "Gun|Accuracy")
    float FollowUpSpreadDeg = 0.6f;

    // ---- Recoil ----
    /** Muzzle flip per shot (degrees up). */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilPitch = 3.2f;

    /** Random sideways kick (+/- degrees). */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilYaw = 0.5f;

    /** Right-handed shooters' muzzle drifts right a little. */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilRightBias = 0.2f;

    /** Shots closer together than this stack recoil... */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilStackWindow = 0.4f;

    /** ...by this much each... */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilStackPerShot = 0.22f;

    /** ...up to this multiple. */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilStackMax = 1.8f;

    /** Recoil x this when aimed (proper grip and stance). */
    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float AimRecoilScale = 0.7f;

    UPROPERTY(EditAnywhere, Category = "Gun|FX")
    float WeaponFOVKick = 2.f;

    /** Peak brightness of the muzzle flash (candelas) and how long it lasts. */
    UPROPERTY(EditAnywhere, Category = "Gun|FX")
    float MuzzleFlashIntensity = 9000.f;

    UPROPERTY(EditAnywhere, Category = "Gun|FX")
    float MuzzleFlashDuration = 0.045f;

    // ---- Noise (how far enemies hear it, cm) ----
    UPROPERTY(EditAnywhere, Category = "Gun|Noise")
    float GunshotNoiseRange = 4500.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Noise")
    float ReloadNoiseRange = 350.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Noise")
    float DryFireNoiseRange = 250.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Visual")
    FName MuzzleSocketName = "Muzzle";

    UPROPERTY(EditAnywhere, Category = "Gun|Visual")
    FVector MuzzleOffsetLocal = FVector(42.f, 8.f, -5.f);

    /** Where the gun sits when fully aimed (camera space, cm): sights up on the eye line. */
    UPROPERTY(EditAnywhere, Category = "Gun|Visual")
    FVector AimLocation = FVector(30.f, 0.f, -3.4f);

    // ---- Hands (operator arms). Camera-aligned frame: X forward, Y right, Z up, cm,
    //      relative to the gun's pivot. Tweak live in PIE until the hands sit on your mesh's grip.
    UPROPERTY(EditAnywhere, Category = "Gun|Hands")
    FVector GripOffset = FVector(-3.f, 0.f, -7.f);

    /** Direction up the grip towards the slide (pistol grips rake back, so this tips forward). */
    UPROPERTY(EditAnywhere, Category = "Gun|Hands")
    FVector GripAxis = FVector(0.25f, 0.f, 1.f);

    /** Support (left) hand relative to the right hand's grip. */
    UPROPERTY(EditAnywhere, Category = "Gun|Hands")
    FVector SupportHandOffset = FVector(1.f, -3.f, -1.f);

    UPROPERTY()
    TSubclassOf<ABullet> BulletClass;

    // Audio (generated by Tools/generate_assets.py; silently skipped if not imported)
    UPROPERTY(EditAnywhere, Category = "Gun|Audio")
    TSoftObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, Category = "Gun|Audio")
    TSoftObjectPtr<USoundBase> DryFireSound;

    UPROPERTY(EditAnywhere, Category = "Gun|Audio")
    TSoftObjectPtr<USoundBase> ReloadSound;

    void PlayGunSound(const TSoftObjectPtr<USoundBase>& Sound, float Volume = 1.f, float PitchJitter = 0.f, float Pitch = 1.f);

    bool bReloading = false;
    bool bEmptyReload = false;
    float LastFireTime = -1000.f;
    float LastShotTime = -1000.f;
    float RecoilStack = 1.f;
    float LastShotRecoilPitch = 0.f;
    float LastShotRecoilYaw = 0.f;
    float MuzzleFlashTimeLeft = 0.f;

    // What the pawn last told us (SetHandling)
    float AimAlpha = 0.f;
    float Move01 = 0.f;
    bool bAirborne = false;
    float Fatigue01 = 0.f;
    bool bCrouched = false;
    float CurrentSpreadDeg = 1.4f;
    float FollowUpSpread = 0.f;

    FTimerHandle TimerHandle_Reload;

    UCameraComponent* EquippedCamera = nullptr;
    FTransform BaseRelativeTransform;
    FTransform CurrentRelativeTransform;
    FRotator BaseCameraRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category = "Gun|FP")
    float FPInterpSpeed = 12.f;

    UPROPERTY(EditAnywhere, Category = "Gun|FP")
    float FP_MaxSwayAngle = 25.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Sway")
    float SwayAmount = 2.5f;

    UPROPERTY(EditAnywhere, Category = "Gun|Sway")
    float SwayInterpSpeed = 5.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Sway")
    float RecoilSwayScale = 0.9f;

    UPROPERTY(EditAnywhere, Category = "Gun|Sway")
    float RecoilSwayDecay = 6.f;

    FRotator CurrentSwayRotation = FRotator::ZeroRotator;
    FRotator MouseSwayRotation = FRotator::ZeroRotator;
    FRotator RecoilSwayRotation = FRotator::ZeroRotator;
};
