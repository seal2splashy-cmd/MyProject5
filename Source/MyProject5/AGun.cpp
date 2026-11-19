#include "AGun.h"
#include "ABullet.h"
#include "AEnemy.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/PackageName.h"
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

    // The old Fab pistol is optional. A ConstructorHelpers lookup logs an Error on every
    // editor start when it isn't in the project, so check the package exists first.
    if (FPackageName::DoesPackageExist(TEXT("/Game/Fab/GUN_Pistol_Orange/pistolorange")))
    {
        ConstructorHelpers::FObjectFinder<UStaticMesh> GunMeshFinder(PATH_GUN_STATIC_MESH);
        if (GunMeshFinder.Succeeded())
        {
            MeshComp->SetStaticMesh(GunMeshFinder.Object);
        }
    }

    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->bCastDynamicShadow = false;
    MeshComp->CastShadow = false;

    // Held out at arm's length, BODYCAM-style. (Was 25,4,-8 for the old Mixamo arms; the operator
    // arms are longer and that crushed both hands against the bottom of the screen.)
    MeshComp->SetRelativeLocation(FVector(38.f, 9.f, -11.f));
    MeshComp->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
    MeshComp->SetRelativeScale3D(FVector(0.6f));

    // Muzzle flash light: off until a shot. Warm, short range, no shadows (it only lasts 3 frames).
    MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
    MuzzleLight->SetupAttachment(MeshComp);
    MuzzleLight->SetIntensityUnits(ELightUnits::Candelas);
    MuzzleLight->SetIntensity(0.f);
    MuzzleLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.42f));
    MuzzleLight->SetAttenuationRadius(1100.f);
    MuzzleLight->SetCastShadows(false);
    MuzzleLight->SetVisibility(false);

    BulletClass = ABullet::StaticClass();

    FireSound    = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Gun_Shot.SW_Gun_Shot")));
    DryFireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Gun_DryFire.SW_Gun_DryFire")));
    ReloadSound  = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Generated/Audio/SW_Gun_Reload.SW_Gun_Reload")));
}

void AGun::PlayGunSound(const TSoftObjectPtr<USoundBase>& Sound, float Volume, float PitchJitter, float Pitch)
{
    if (USoundBase* S = Sound.LoadSynchronous())
    {
        const float FinalPitch = Pitch * (1.f + FMath::FRandRange(-PitchJitter, PitchJitter));
        UGameplayStatics::PlaySoundAtLocation(this, S, GetActorLocation(), Volume, FinalPitch);
    }
}

void AGun::BeginPlay()
{
    Super::BeginPlay();

    // Visual mesh, best available first. None of these are hard dependencies: whichever
    // is imported wins, otherwise the constructor's mesh and offsets stay as they are.
    auto LoadOptionalMesh = [](const TCHAR* PackageName, const TCHAR* ObjectPath) -> UStaticMesh*
    {
        // Check first so a missing asset doesn't log a load error every Play.
        return FPackageName::DoesPackageExist(PackageName)
            ? Cast<UStaticMesh>(FSoftObjectPath(ObjectPath).TryLoad())
            : nullptr;
    };

    // 1. Meshy pistol (Tools/prepare_meshy_weapons_blender.py -> Tools/import_meshy_weapons_ue58.py).
    //    18.7 cm, pivot placed so the grip centre is at GripOffset; numbers measured by that script.
    if (UStaticMesh* MeshyPistol = LoadOptionalMesh(
            TEXT("/Game/Generated/Weapons/SM_AH_Pistol"),
            TEXT("/Game/Generated/Weapons/SM_AH_Pistol.SM_AH_Pistol")))
    {
        MeshComp->SetStaticMesh(MeshyPistol);
        MeshComp->SetRelativeLocation(FVector(38.f, 9.f, -11.f));
        MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
        MeshComp->SetRelativeScale3D(FVector::OneVector);

        GripOffset = FVector(-3.f, 0.f, -7.f);
        GripAxis = FVector(0.35f, 0.f, 0.94f);
        SupportHandOffset = FVector(1.f, -3.f, -1.f);
        MuzzleOffsetLocal = FVector(11.86f, 0.f, -0.84f);
        AimLocation = FVector(30.f, 0.f, -2.6f);   // bore 0.84 below the pivot, sights ~2.4 above the bore

        UE_LOG(LogTemp, Log, TEXT("AGun: Meshy pistol mesh loaded."));
    }
    // 2. Procedural service pistol (Tools/build_pistol_blender.py -> Tools/import_pistol_ue58.py).
    //    Unreal cm, +X out of the muzzle, +Y right, +Z up; pivot placed so the grip centre is
    //    at GripOffset below. All numbers here come from that script.
    else if (UStaticMesh* Pistol = LoadOptionalMesh(
            TEXT("/Game/Generated/Weapons/SM_AH_ServicePistol"),
            TEXT("/Game/Generated/Weapons/SM_AH_ServicePistol.SM_AH_ServicePistol")))
    {
        MeshComp->SetStaticMesh(Pistol);
        MeshComp->SetRelativeLocation(FVector(38.f, 9.f, -11.f));
        MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
        MeshComp->SetRelativeScale3D(FVector::OneVector);

        GripOffset = FVector(-3.f, 0.f, -7.f);           // centre of the grip
        GripAxis = FVector(0.33f, 0.f, 0.94f);           // up the raked grip towards the slide
        SupportHandOffset = FVector(1.f, -3.f, -1.f);    // support hand wraps the firing hand
        MuzzleOffsetLocal = FVector(14.42f, 0.f, 0.3f);  // bore exit
        AimLocation = FVector(30.f, 0.f, -3.6f);

        UE_LOG(LogTemp, Log, TEXT("AGun: service pistol mesh loaded."));
    }
    // 3. Operator field sidearm prop (Tools/import_operator_sidearm_ue58.py).
    else if (UStaticMesh* SidearmVisual = LoadOptionalMesh(
            TEXT("/Game/Generated/Operator/Props/SM_AH_FieldSidearm"),
            TEXT("/Game/Generated/Operator/Props/SM_AH_FieldSidearm.SM_AH_FieldSidearm")))
    {
        MeshComp->SetStaticMesh(SidearmVisual);

        // Modelled in Unreal centimetres: +X forwards, +Y right, +Z up.
        // Unlike the previous asset, it needs no 90-degree mesh correction.
        MeshComp->SetRelativeLocation(FVector(38.f, 9.f, -11.f));
        MeshComp->SetRelativeRotation(FRotator::ZeroRotator);
        MeshComp->SetRelativeScale3D(FVector::OneVector);

        // Separate primary and support grips placed on the model itself.
        // The original hands were only ~3 cm apart, causing overlap.
        GripOffset = FVector(-5.f, 0.f, -10.f);
        SupportHandOffset = FVector(21.f, -9.f, 1.f);
        GripAxis = FVector::UpVector;
        MuzzleOffsetLocal = FVector(42.6f, 0.f, 1.f);    // its barrel tip
        AimLocation = FVector(24.f, 0.f, -4.f);

        UE_LOG(LogTemp, Log, TEXT("AGun: optional operator sidearm visual loaded."));
    }
    // 4. Otherwise: the old Fab pistol from the constructor, if it's in the project.

    // Flash sits just in front of whichever muzzle we ended up with
    if (MuzzleLight && MeshComp)
    {
        if (MeshComp->DoesSocketExist(MuzzleSocketName))
        {
            MuzzleLight->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, MuzzleSocketName);
        }
        else
        {
            MuzzleLight->SetRelativeLocation(MuzzleOffsetLocal + FVector(4.f, 0.f, 0.f));
        }
    }
}

int32 AGun::GetReserveAmmo() const
{
    int32 Total = 0;
    for (const int32 Rounds : SpareMags)
    {
        Total += Rounds;
    }
    return Total;
}

void AGun::SetHandling(float InAimAlpha, float InMove01, bool bInAir, float InFatigue01, bool bInCrouched)
{
    AimAlpha = FMath::Clamp(InAimAlpha, 0.f, 1.f);
    Move01 = FMath::Clamp(InMove01, 0.f, 1.f);
    bAirborne = bInAir;
    Fatigue01 = FMath::Clamp(InFatigue01, 0.f, 1.f);
    bCrouched = bInCrouched;

    // Accuracy right now. Steadier crouched; worst in the air or at a run.
    const UWorld* W = GetWorld();
    const float Dt = W ? W->GetDeltaSeconds() : 0.016f;
    FollowUpSpread = FMath::FInterpTo(FollowUpSpread, 0.f, Dt, 3.f);
    float Spread = FMath::Lerp(HipSpreadDeg, AimSpreadDeg, AimAlpha);
    Spread += MoveSpreadDeg * Move01 * FMath::Lerp(1.f, 0.6f, AimAlpha);
    Spread += Fatigue01 * FatigueSpreadDeg;
    Spread += FollowUpSpread;
    if (bAirborne) Spread += AirSpreadDeg;
    if (bCrouched) Spread *= 0.8f;
    CurrentSpreadDeg = Spread;
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

    if (!bRoundChambered)
    {
        LastFireTime = Now;                 // rate-limit the click too
        PlayGunSound(DryFireSound, 0.8f);
        AEnemy::ReportNoise(this, GetActorLocation(), DryFireNoiseRange, OwningPawn);
        StartReload();
        return false;
    }

    // Fire the chambered round; the slide cycles and chambers the next one if there is one.
    // If the magazine was empty the slide locks back (shown as 0 / reload from empty).
    bRoundChambered = AmmoInMag > 0;
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

    // Everything within ~45 m hears that (half that through walls).
    AEnemy::ReportNoise(this, MuzzleWorldLoc, GunshotNoiseRange, OwningPawn);
    MuzzleFlashTimeLeft = MuzzleFlashDuration;
    if (MuzzleLight)
    {
        MuzzleLight->SetVisibility(true);
        MuzzleLight->SetIntensity(MuzzleFlashIntensity * FMath::FRandRange(0.8f, 1.1f));
    }

    const float MuzzleForwardOffset = 12.f;
    const FVector SpawnLoc = MuzzleWorldLoc + EyeRotation.Vector() * MuzzleForwardOffset;

    // Crosshair/bullet alignment fix: bullets travel towards the SAME point the
    // crosshair is projected from, so the two can never disagree. Then the accuracy cone.
    const FVector AimPoint = GetMuzzleAimPoint(8000.f);
    FVector BulletDir = (AimPoint - SpawnLoc).GetSafeNormal(UE_SMALL_NUMBER, EyeRotation.Vector());
    BulletDir = FMath::VRandCone(BulletDir, FMath::DegreesToRadians(FMath::Max(CurrentSpreadDeg, 0.01f)));
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

    // Recoil: quick follow-ups stack, a proper aimed stance tames it, being out of breath doesn't help.
    const bool bFollowUp = (Now - LastShotTime) < RecoilStackWindow;
    RecoilStack = bFollowUp ? FMath::Min(RecoilStack + RecoilStackPerShot, RecoilStackMax) : 1.f;
    LastShotTime = Now;
    const float Scale = RecoilStack
        * FMath::Lerp(1.f, AimRecoilScale, AimAlpha)
        * (bCrouched ? 0.85f : 1.f)
        * (1.f + 0.3f * Fatigue01);
    LastShotRecoilPitch = RecoilPitch * Scale * FMath::FRandRange(0.9f, 1.1f);
    LastShotRecoilYaw = (RecoilRightBias + FMath::FRandRange(-RecoilYaw, RecoilYaw)) * Scale;
    FollowUpSpread = FMath::Min(FollowUpSpread + FollowUpSpreadDeg, FollowUpSpreadDeg * 4.f);

    {
        // The gun itself flips up in the hands (positive pitch = nose up)
        const float PitchKick = LastShotRecoilPitch * RecoilSwayScale;
        const float YawKick = LastShotRecoilYaw * RecoilSwayScale;

        RecoilSwayRotation.Pitch += PitchKick;
        RecoilSwayRotation.Yaw += YawKick;

        RecoilSwayRotation.Pitch = FMath::Clamp(RecoilSwayRotation.Pitch, -FP_MaxSwayAngle, FP_MaxSwayAngle);
        RecoilSwayRotation.Yaw = FMath::Clamp(RecoilSwayRotation.Yaw, -FP_MaxSwayAngle, FP_MaxSwayAngle);
    }

    return true;
}

void AGun::StartReload()
{
    if (bReloading) return;

    // Anything worth loading? (A mag with more rounds than the one in the gun.)
    int32 Best = INDEX_NONE;
    for (int32 i = 0; i < SpareMags.Num(); ++i)
    {
        if (SpareMags[i] > 0 && (Best == INDEX_NONE || SpareMags[i] > SpareMags[Best]))
        {
            Best = i;
        }
    }
    if (Best == INDEX_NONE) return;
    if (SpareMags[Best] <= AmmoInMag && bRoundChambered) return;   // already as full as it gets

    bReloading = true;
    bEmptyReload = !bRoundChambered;
    // An empty reload ends with the slide being released, so it takes longer (sound stretched to match)
    PlayGunSound(ReloadSound, 0.9f, 0.f, bEmptyReload ? 0.85f : 1.05f);
    AEnemy::ReportNoise(this, GetActorLocation(), ReloadNoiseRange, GetOwner());
    GetWorld()->GetTimerManager().SetTimer(TimerHandle_Reload, this, &AGun::FinishReload,
        bEmptyReload ? EmptyReloadTime : ReloadTime, false);
}

void AGun::FinishReload()
{
    bReloading = false;

    int32 Best = INDEX_NONE;
    for (int32 i = 0; i < SpareMags.Num(); ++i)
    {
        if (SpareMags[i] > 0 && (Best == INDEX_NONE || SpareMags[i] > SpareMags[Best]))
        {
            Best = i;
        }
    }
    if (Best == INDEX_NONE) return;

    // The mag coming out goes back in the pouch with whatever is left in it (empties are dropped).
    const int32 OldMag = AmmoInMag;
    AmmoInMag = SpareMags[Best];
    SpareMags.RemoveAt(Best);
    if (OldMag > 0)
    {
        SpareMags.Add(OldMag);
    }

    // From empty: releasing the slide strips a round off the new mag into the chamber.
    if (!bRoundChambered && AmmoInMag > 0)
    {
        --AmmoInMag;
        bRoundChambered = true;
    }
    bEmptyReload = false;
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

void AGun::UpdateMuzzleFlash(float DeltaTime)
{
    if (!MuzzleLight || MuzzleFlashTimeLeft <= 0.f)
    {
        return;
    }
    MuzzleFlashTimeLeft -= DeltaTime;
    if (MuzzleFlashTimeLeft <= 0.f)
    {
        MuzzleLight->SetIntensity(0.f);
        MuzzleLight->SetVisibility(false);
    }
    else
    {
        // fades over its short life
        MuzzleLight->SetIntensity(MuzzleFlashIntensity * (MuzzleFlashTimeLeft / FMath::Max(MuzzleFlashDuration, 0.001f)));
    }
}

void AGun::ApplyFirstPersonOffset(const FVector& CameraLocalPosOffset, const FRotator& CameraLocalRotOffset)
{
    if (!EquippedCamera) return;
    UWorld* W = GetWorld();
    if (!W) return;
    const float DeltaTime = W->GetDeltaSeconds();

    UpdateMuzzleFlash(DeltaTime);

    // Aiming brings the sights up to the eye line (MeshComp was set up at the hip position).
    const FVector HipLoc = BaseRelativeTransform.GetLocation();
    FVector DesiredLoc = FMath::Lerp(HipLoc, AimLocation, AimAlpha) + CameraLocalPosOffset;
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

bool AGun::GetHandTargets(FVector& OutRightGrip, FVector& OutLeftGrip, FVector& OutAxis) const
{
    if (!EquippedCamera || IsHidden())
    {
        return false;
    }

    // The gun's sway, as a rotation in camera space (current pose relative to its resting pose).
    // Using the delta keeps GripOffset in an intuitive camera-aligned frame, independent of the
    // mesh's baked-in 90 degree yaw.
    const FQuat SwayDelta = CurrentRelativeTransform.GetRotation() * BaseRelativeTransform.GetRotation().Inverse();
    const FVector Pivot = CurrentRelativeTransform.GetLocation();
    const FTransform& Cam = EquippedCamera->GetComponentTransform();

    OutRightGrip = Cam.TransformPosition(Pivot + SwayDelta.RotateVector(GripOffset));
    OutLeftGrip  = Cam.TransformPosition(Pivot + SwayDelta.RotateVector(GripOffset + SupportHandOffset));
    OutAxis      = Cam.TransformVectorNoScale(SwayDelta.RotateVector(GripAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector)));
    return true;
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
    if (MuzzleLight && bShouldHide)
    {
        MuzzleLight->SetVisibility(false);
        MuzzleFlashTimeLeft = 0.f;
    }
}
