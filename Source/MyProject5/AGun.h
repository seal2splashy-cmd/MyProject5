#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AGun.generated.h"

class UStaticMeshComponent;
class ABullet;
class UCameraComponent;

UCLASS()
class MYPROJECT5_API AGun : public AActor
{
    GENERATED_BODY()

public:
    AGun();

    bool Fire(const FVector& EyeLocation, const FRotator& EyeRotation, AController* InstigatorController, APawn* OwningPawn);
    void StartReload();
    bool IsReloading() const { return bReloading; }

    float GetRecoilPitch() const { return RecoilPitch; }
    float GetRecoilYaw()   const { return RecoilYaw; }
    float GetWeaponFOVKick() const { return WeaponFOVKick; }

    int32 GetAmmoInMag() const { return AmmoInMag; }
    int32 GetReserveAmmo() const { return ReserveAmmo; }

    void EquipToCamera(UCameraComponent* Camera, const FTransform& RelativeTransform = FTransform::Identity);
    void ApplyFirstPersonOffset(const FVector& CameraLocalPosOffset, const FRotator& CameraLocalRotOffset);

    // Returns a world-space point along the gun's aim direction (camera forward + sway).
    // Used by the crosshair AND by Fire(), so both share one source of truth.
    FVector GetMuzzleAimPoint(float Distance = 10000.f) const;

    /** Mesh the first-person arms attach to, so they inherit every bit of sway. */
    UStaticMeshComponent* GetGunMesh() const { return MeshComp; }

    /** Hide/show the gun (used when swapping to melee). */
    void SetGunHidden(bool bHidden);

protected:
    virtual void BeginPlay() override;

private:
    bool GetMuzzleTransform(FVector& OutLoc, FRotator& OutRot) const;
    void FinishReload();

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* MeshComp;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float Damage = 28.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    int32 MagSize = 12;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    int32 AmmoInMag = 12;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    int32 ReserveAmmo = 36;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float FireRate = 4.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Stats")
    float ReloadTime = 1.4f;

    UPROPERTY(EditAnywhere, Category = "Gun|Projectile")
    float MuzzleVelocity = 35000.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilPitch = 3.6f;

    UPROPERTY(EditAnywhere, Category = "Gun|Recoil")
    float RecoilYaw = 0.6f;

    UPROPERTY(EditAnywhere, Category = "Gun|FX")
    float WeaponFOVKick = 5.f;

    UPROPERTY(EditAnywhere, Category = "Gun|Visual")
    FName MuzzleSocketName = "Muzzle";

    UPROPERTY(EditAnywhere, Category = "Gun|Visual")
    FVector MuzzleOffsetLocal = FVector(42.f, 8.f, -5.f);

    UPROPERTY()
    TSubclassOf<ABullet> BulletClass;

    bool bReloading = false;
    float LastFireTime = -1000.f;

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
