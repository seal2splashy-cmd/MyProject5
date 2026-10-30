#include "AGun.h"
#include "ABullet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

#define PATH_GUN_STATIC_MESH TEXT("/Game/Fab/GUN_Pistol_Orange/pistolorange.pistolorange")

static float SafeCooldownFromRate(float FireRate)
{
    return (FireRate > 0.f) ? (1.f / FireRate) : 0.f;
}

AGun::AGun()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;

    {
        ConstructorHelpers::FObjectFinder<UStaticMesh> GunMeshFinder(PATH_GUN_STATIC_MESH);
        if (GunMeshFinder.Succeeded())
        {
            MeshComp->SetStaticMesh(GunMeshFinder.Object);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("AGun: failed to load mesh at %s"), PATH_GUN_STATIC_MESH);
        }
    }

    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->bCastDynamicShadow = false;
    MeshComp->CastShadow = false;

    MeshComp->SetRelativeLocation(FVector(25.f, 4.f, -8.f));
    MeshComp->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
    MeshComp->SetRelativeScale3D(FVector(0.6f));

    BulletClass = ABullet::StaticClass();

    FireSound    = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Gun_Shot.SW_Gun_Shot")));
    DryFireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Gun_DryFire.SW_Gun_DryFire")));
    ReloadSound  = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Gun_Reload.SW_Gun_Reload")));
}

void AGun::PlayGunSound(const TSoftObjectPtr<USoundBase>& Sound, float Volume, float PitchJitter)
{
    if (USoundBase* S = Sound.LoadSynchronous())
    {
        const float Pitch = 1.f + FMath::FRandRange(-PitchJitter, PitchJitter);
        UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), Volume, Pitch);
    }
}

void AGun::BeginPlay()
{
    Super::BeginPlay();
}

bool AGun::GetMuzzleTransform(FVector& OutLoc, FRotator& OutRot) const
{
    if (MeshComp && MeshComp->DoesSocketExist(MuzzleSocketName))
    {
        const FTransform S = MeshComp->GetSocketTransform(MuzzleSocketName, ERelativeTransformSpace::RTS_World);
        OutLoc = S.GetLocation();
        OutRot = S.Rotator();
        return true;
    }

    if (MeshComp)
    {
        const FTransform T = MeshComp->GetComponentTransform();
        OutLoc = T.TransformPosition(MuzzleOffsetLocal);
        OutRot = T.Rotator();
        return true;
    }
    return false;
}

bool AGun::Fire(const FVector& EyeLocation, const FRotator& EyeRotation, AController* /*InstigatorController*/, APawn* OwningPawn)
{
    if (bReloading) return false;
    if (MeshComp && !MeshComp->IsVisible()) return false; // holstered

    UWorld* W = GetWorld();
    if (!W) return false;

    const float Now = W->GetTimeSeconds();
    const float Cooldown = SafeCooldownFromRate(FireRate);
    if ((Now - LastFireTime) < Cooldown)
        return false;

    if (AmmoInMag <= 0)
    {
        LastFireTime = Now;                 // rate-limit the click too
        PlayGunSound(DryFireSound, 0.8f);
        StartReload();
        return false;
    }

    AmmoInMag = FMath::Max(0, AmmoInMag - 1);
    LastFireTime = Now;
    PlayGunSound(FireSound, 1.f, 0.04f);

    FVector MuzzleWorldLoc;
    FRotator MuzzleWorldRot;
    if (!GetMuzzleTransform(MuzzleWorldLoc, MuzzleWorldRot))
    {
        MuzzleWorldLoc = EyeLocation;
        MuzzleWorldRot = EyeRotation;
    }

    const float MuzzleForwardOffset = 12.f;
    const FVector SpawnLoc = MuzzleWorldLoc + EyeRotation.Vector() * MuzzleForwardOffset;

    // Crosshair/bullet alignment fix: bullets travel towards the SAME point the
    // crosshair is projected from, so the two can never disagree.
    const FVector AimPoint = GetMuzzleAimPoint(8000.f);
    const FVector BulletDir = (AimPoint - SpawnLoc).GetSafeNormal(UE_SMALL_NUMBER, EyeRotation.Vector());
    const FRotator SpawnRot = BulletDir.Rotation();

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = OwningPawn;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (BulletClass)
    {
        ABullet* Bullet = W->SpawnActor<ABullet>(BulletClass, SpawnLoc, SpawnRot, Params);
        if (Bullet)
        {
            Bullet->Init(BulletDir * MuzzleVelocity, Damage);
        }
    }

    {
        const float PitchKick = RecoilPitch * RecoilSwayScale;
        const float YawKick = FMath::FRandRange(-RecoilYaw, RecoilYaw) * RecoilSwayScale;

        RecoilSwayRotation.Pitch += -PitchKick;
        RecoilSwayRotation.Yaw += YawKick;

        RecoilSwayRotation.Pitch = FMath::Clamp(RecoilSwayRotation.Pitch, -FP_MaxSwayAngle, FP_MaxSwayAngle);
        RecoilSwayRotation.Yaw = FMath::Clamp(RecoilSwayRotation.Yaw, -FP_MaxSwayAngle, FP_MaxSwayAngle);
    }

    return true;
}

void AGun::StartReload()
{
    if (bReloading) return;
    if (AmmoInMag >= MagSize) return;
    if (ReserveAmmo <= 0) return;

    bReloading = true;
    PlayGunSound(ReloadSound, 0.9f);
    GetWorld()->GetTimerManager().SetTimer(TimerHandle_Reload, this, &AGun::FinishReload, ReloadTime, false);
}

void AGun::FinishReload()
{
    const int32 Needed = MagSize - AmmoInMag;
    const int32 ToLoad = FMath::Min(Needed, ReserveAmmo);
    AmmoInMag += ToLoad;
    ReserveAmmo -= ToLoad;
    bReloading = false;
}

void AGun::EquipToCamera(UCameraComponent* Camera, const FTransform& RelativeTransform)
{
    if (!Camera) return;

    EquippedCamera = Camera;
    AttachToComponent(Camera, FAttachmentTransformRules::KeepRelativeTransform);

    if (RelativeTransform.Equals(FTransform::Identity))
    {
        if (USceneComponent* Root = GetRootComponent())
        {
            BaseRelativeTransform = Root->GetRelativeTransform();
        }
        else
        {
            BaseRelativeTransform = GetActorTransform().GetRelativeTransform(Camera->GetComponentTransform());
        }
    }
    else
    {
        BaseRelativeTransform = RelativeTransform;
        if (USceneComponent* Root = GetRootComponent())
        {
            Root->SetRelativeTransform(BaseRelativeTransform);
        }
        else
        {
            SetActorTransform(Camera->GetComponentTransform() * BaseRelativeTransform);
        }
    }

    CurrentRelativeTransform = BaseRelativeTransform;
    BaseCameraRotation = Camera->GetComponentRotation();
}

void AGun::ApplyFirstPersonOffset(const FVector& CameraLocalPosOffset, const FRotator& CameraLocalRotOffset)
{
    if (!EquippedCamera) return;
    UWorld* W = GetWorld();
    if (!W) return;
    const float DeltaTime = W->GetDeltaSeconds();

    FVector DesiredLoc = BaseRelativeTransform.GetLocation() + CameraLocalPosOffset;
    FRotator DesiredRot = BaseRelativeTransform.GetRotation().Rotator() + CameraLocalRotOffset;

    // Note: the camera's owner is the pawn, not the controller, so this cast never
    // succeeds and mouse sway here stays zero. That's how your build behaved - the
    // pawn's UpdateGunSway does the real mouse sway. Kept as-is so the feel is unchanged.
    MouseSwayRotation = FRotator::ZeroRotator;
    if (APlayerController* PC = Cast<APlayerController>(EquippedCamera->GetOwner()))
    {
        float MouseX = 0.f, MouseY = 0.f;
        PC->GetInputMouseDelta(MouseX, MouseY);

        MouseSwayRotation.Pitch = FMath::Clamp(MouseY * SwayAmount, -FP_MaxSwayAngle, FP_MaxSwayAngle);
        MouseSwayRotation.Yaw = FMath::Clamp(-MouseX * SwayAmount, -FP_MaxSwayAngle, FP_MaxSwayAngle);
    }

    RecoilSwayRotation = FMath::RInterpTo(RecoilSwayRotation, FRotator::ZeroRotator, DeltaTime, RecoilSwayDecay);
    FRotator CombinedTarget = MouseSwayRotation + RecoilSwayRotation;
    CurrentSwayRotation = FMath::RInterpTo(CurrentSwayRotation, CombinedTarget, DeltaTime, SwayInterpSpeed);

    FRotator FinalRot = DesiredRot + CurrentSwayRotation;

    FVector NewLoc = FMath::VInterpTo(CurrentRelativeTransform.GetLocation(), DesiredLoc, DeltaTime, FPInterpSpeed);
    FQuat CurrQ = CurrentRelativeTransform.GetRotation();
    FQuat TargetQ = FinalRot.Quaternion();
    const float Alpha = FMath::Clamp(FPInterpSpeed * DeltaTime, 0.f, 1.f);
    FQuat NewQ = FQuat::Slerp(CurrQ, TargetQ, Alpha);

    CurrentRelativeTransform.SetLocation(NewLoc);
    CurrentRelativeTransform.SetRotation(NewQ);
    CurrentRelativeTransform.SetScale3D(BaseRelativeTransform.GetScale3D());

    if (USceneComponent* Root = GetRootComponent())
    {
        Root->SetRelativeTransform(CurrentRelativeTransform);
    }
    else
    {
        SetActorTransform(EquippedCamera->GetComponentTransform() * CurrentRelativeTransform);
    }
}

FVector AGun::GetMuzzleAimPoint(float Distance) const
{
    // Muzzle world POSITION only - the mesh rotation has a baked-in 90 degree offset
    FVector MuzzleLoc;
    FRotator Unused;
    if (!GetMuzzleTransform(MuzzleLoc, Unused))
    {
        MuzzleLoc = GetActorLocation();
    }

    // Direction = camera forward + current sway, so the crosshair moves with the gun
    FRotator AimRot = EquippedCamera
        ? EquippedCamera->GetComponentRotation()
        : GetActorRotation();

    AimRot += CurrentSwayRotation;

    return MuzzleLoc + AimRot.Vector() * Distance;
}

void AGun::SetGunHidden(bool bShouldHide)
{
    SetActorHiddenInGame(bShouldHide);
    if (MeshComp)
    {
        // propagate so the attached arms hide with it
        MeshComp->SetVisibility(!bShouldHide, true);
    }
}
