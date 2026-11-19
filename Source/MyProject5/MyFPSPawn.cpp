#include "MyFPSPawn.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "AEnemy.h"
#include "AGun.h"
#include "MeleeCombatComponent.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Components/LocalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "CoreGlobals.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "UObject/UObjectIterator.h"
#include "MeleeWeapon.h"
#include "OperatorArmsComponent.h"
#include "BodycamLensComponent.h"
#include "BodycamTorchComponent.h"
#include "RealismUtils.h"

AMyFPSPawn::AMyFPSPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
    RootComponent = CapsuleComponent;
    CapsuleComponent->InitCapsuleSize(44.f, StandingHalfHeight);

    CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
    CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Block);
    CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    CapsuleComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(RootComponent);
    CameraComponent->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
    CameraComponent->FieldOfView = BaseFOV;
    CameraComponent->bUsePawnControlRotation = false;

    // Near clip: set project-wide in Config/DefaultEngine.ini (NearClipPlane=1.0) so the
    // arms/gun don't get cut off in front of the lens. The per-camera override members
    // don't exist on UCameraComponent in this engine version.

    // First-person arms - re-attached to the gun mesh in BeginPlay
    ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
    ArmsMesh->SetupAttachment(CameraComponent);
    ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ArmsMesh->CastShadow = false;
    ArmsMesh->bCastDynamicShadow = false;
    ArmsMesh->SetOnlyOwnerSee(true);

    ArmsMeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/character.character")));
    ArmsIdleAnim  = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Ch35_nonPBR__1_.Ch35_nonPBR__1_")));
    ArmsWalkAnim  = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Pistol_Walk.Pistol_Walk")));

    // IK-driven first-person arms (Anomaly Hunter Containment Operator)
    OperatorArms = CreateDefaultSubobject<UOperatorArmsComponent>(TEXT("OperatorArms"));
    OperatorArms->SetupAttachment(CameraComponent);

    // Physics melee
    Melee = CreateDefaultSubobject<UMeleeCombatComponent>(TEXT("Melee"));

    // Bodycam lens look (post-process on the camera) and chest torch
    BodycamLens = CreateDefaultSubobject<UBodycamLensComponent>(TEXT("BodycamLens"));
    Torch = CreateDefaultSubobject<UBodycamTorchComponent>(TEXT("Torch"));
    Torch->SetupAttachment(CameraComponent);

    // Generated audio
    auto Snd = [](const TCHAR* Name)
    {
        return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Generated/Audio/%s.%s"), Name, Name)));
    };
    FootstepSounds = { Snd(TEXT("SW_Footstep_Concrete_01")), Snd(TEXT("SW_Footstep_Concrete_02")),
                       Snd(TEXT("SW_Footstep_Concrete_03")), Snd(TEXT("SW_Footstep_Concrete_04")) };
    HurtSound      = Snd(TEXT("SW_Player_Hurt"));
    AmbienceSound  = Snd(TEXT("SW_Ambience_BodycamHiss_Loop"));
    BreathSound    = Snd(TEXT("SW_Breath_Heavy_Loop"));
    HeartbeatSound = Snd(TEXT("SW_Heartbeat_Loop"));

    AutoPossessPlayer = EAutoReceiveInput::Player0;
    bUseControllerRotationYaw = true;

    CurrentSpeed = WalkSpeed;
    DesiredSpeed = WalkSpeed;

    TargetCapsuleHalfHeight = StandingHalfHeight;
    TargetCameraZ = 60.f;
    CameraBaseZ = 60.f;

    Stamina = MaxStamina;

    RecoilCurrent = FRotator::ZeroRotator;
    FOVAdd = 0.f;
    bIsAiming = false;

    GunClass = AGun::StaticClass();
}

void AMyFPSPawn::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        SmoothedCameraRotation = PC->GetControlRotation();
        PreviousControlYaw = PC->GetControlRotation().Yaw;
        PreviousControlPitch = PC->GetControlRotation().Pitch;
    }
    CameraComponent->SetFieldOfView(BaseFOV);
    BodycamLens->InitLens(CameraComponent);

    if (GunClass)
    {
        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        CurrentGun = GetWorld()->SpawnActor<AGun>(GunClass, Params);
        if (CurrentGun)
        {
            CurrentGun->EquipToCamera(CameraComponent);
            CurrentGun->SetOwner(this);
        }
    }

    CurrentHealth = MaxHealth;
    bIsDead = false;

    SetupArms();

    // Constant bodycam mic hiss (the imported wave is set to loop)
    if (USoundBase* Amb = AmbienceSound.LoadSynchronous())
    {
        AmbienceComponent = UGameplayStatics::SpawnSound2D(this, Amb, AmbienceVolume);
    }
    // Breathing loop: always running, faded in and out with how tired you are
    if (USoundBase* Breath = BreathSound.LoadSynchronous())
    {
        BreathComponent = UGameplayStatics::SpawnSound2D(this, Breath, 0.01f, 1.f, 0.f, nullptr, false, false);
    }
    // Heartbeat loop: only heard when you're scared; faster (pitched up) the more scared you are
    if (USoundBase* Heart = HeartbeatSound.LoadSynchronous())
    {
        HeartbeatComponent = UGameplayStatics::SpawnSound2D(this, Heart, 0.01f, 1.f, 0.f, nullptr, false, false);
    }
}

FVector AMyFPSPawn::GetPawnViewLocation() const
{
    return CameraComponent ? CameraComponent->GetComponentLocation() : GetActorLocation();
}

// ------------------------------------------------------------------ visibility (stealth)

bool AMyFPSPawn::IsTorchShiningAt(const FVector& Point) const
{
    if (!Torch || !Torch->IsTorchOn() || Torch->Intensity <= 1.f)
    {
        return false;
    }
    const FVector From = Torch->GetComponentLocation();
    const FVector To = Point - From;
    const float Dist = To.Size();
    if (Dist < 1.f || Dist > Torch->AttenuationRadius)
    {
        return false;
    }
    const float CosAngle = FVector::DotProduct(Torch->GetForwardVector(), To / Dist);
    return CosAngle >= FMath::Cos(FMath::DegreesToRadians(Torch->OuterConeAngle));
}

void AMyFPSPawn::UpdateVisibility(float DeltaTime)
{
    VisibilityTimer -= DeltaTime;
    if (VisibilityTimer > 0.f)
    {
        return;
    }
    VisibilityTimer = 0.2f;   // a few times a second is plenty

    UWorld* World = GetWorld();
    const FVector Me = GetActorLocation();
    float Light = AmbientVisibility;

    // Every lamp in range that can actually see us adds to how lit we are (inverse-square-ish falloff).
    for (TObjectIterator<ULocalLightComponent> It; It; ++It)
    {
        const ULocalLightComponent* Lamp = *It;
        if (!Lamp || Lamp->GetWorld() != World || Lamp == Torch || !Lamp->IsVisible() || Lamp->Intensity <= 0.f)
        {
            continue;
        }
        if (CurrentGun && Lamp->GetOwner() == CurrentGun)
        {
            continue;   // the muzzle flash doesn't count
        }
        const FVector LampLoc = Lamp->GetComponentLocation();
        const float Radius = Lamp->AttenuationRadius;
        const float Dist = FVector::Dist(LampLoc, Me);
        if (Radius <= 0.f || Dist >= Radius)
        {
            continue;
        }
        if (const USpotLightComponent* Spot = Cast<USpotLightComponent>(Lamp))
        {
            const FVector ToMe = (Me - LampLoc) / FMath::Max(Dist, 1.f);
            if (FVector::DotProduct(Spot->GetForwardVector(), ToMe) < FMath::Cos(FMath::DegreesToRadians(Spot->OuterConeAngle)))
            {
                continue;
            }
        }
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(PlayerLightCheck), false, this);
        if (const AActor* LampOwner = Lamp->GetOwner())
        {
            Params.AddIgnoredActor(LampOwner);
        }
        if (World->LineTraceSingleByChannel(Hit, LampLoc, Me, ECC_Visibility, Params))
        {
            continue;   // in its shadow
        }
        const float Falloff = FMath::Square(1.f - FMath::Square(Dist / Radius));
        Light += Falloff * LampVisibility;
    }

    if (Torch && Torch->IsTorchOn())
    {
        Light += TorchVisibility;
    }
    if (bIsCrouching)
    {
        Light *= 0.7f;
    }
    if (PlanarVelocity.Size() > WalkSpeed * 1.2f)
    {
        Light += 0.1f;   // movement catches the eye
    }
    Visibility = FMath::Clamp(Light, 0.f, 1.f);
}

// ------------------------------------------------------------------ footsteps / breathing

void AMyFPSPawn::PlayFootstep(float Volume, bool bLanding)
{
    const float HalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
    const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
    const AHRealism::ESurface Surface = AHRealism::SurfaceUnder(GetWorld(), Feet, 30.f, this);
    const AHRealism::FSurfaceInfo& Info = AHRealism::Info(Surface);

    USoundBase* Sound = AHRealism::RandomVariant(Info.FootstepSound, Info.FootstepVariants);
    if (!Sound && FootstepSounds.Num() > 0)
    {
        Sound = FootstepSounds[FMath::RandRange(0, FootstepSounds.Num() - 1)].LoadSynchronous();
    }
    if (Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, Feet, Volume * FootstepVolume * FMath::Clamp(Info.FootstepLoudness, 0.6f, 1.3f),
            FMath::FRandRange(0.92f, 1.08f));
    }

    // Enemies hear it: louder running, on metal, landing; silent sneaking
    float Range = bLanding ? LandNoiseRange * Volume
        : (bIsCrouching ? CrouchNoiseRange : (bIsRunning ? SprintNoiseRange : WalkNoiseRange));
    Range *= Info.FootstepLoudness;
    if (Range > 1.f)
    {
        AEnemy::ReportNoise(this, Feet, Range, this);
    }
}

void AMyFPSPawn::UpdateFootsteps()
{
    // One step per head-bob cycle, landing on the bottom of the bob (sin = -1 at 1.5 pi)
    if (!bEnableHeadBob || !bIsOnGround || bIsSliding)
    {
        return;
    }
    const int32 StepIndex = FMath::FloorToInt((HeadBobTimer + 0.5f * UE_PI) / (2.f * UE_PI));
    if (StepIndex != LastStepIndex)
    {
        LastStepIndex = StepIndex;
        float Vol = 0.7f;
        if (bIsRunning)   Vol = 1.f;
        if (bIsCrouching) Vol = 0.3f;
        PlayFootstep(Vol);
    }
}

void AMyFPSPawn::UpdateBreathing(float DeltaTime)
{
    // Heavier the more tired (and the more hurt) you are; builds fast, settles slowly
    const float Tired = 1.f - Stamina / FMath::Max(MaxStamina, 1.f);
    const float Hurt = 1.f - CurrentHealth / FMath::Max(MaxHealth, 1.f);
    const float Target = bIsDead ? 0.f : FMath::Clamp(Tired * 1.25f - 0.15f + Hurt * 0.35f + Fear * 0.4f, 0.f, 1.f);
    BreathLevel = FMath::FInterpTo(BreathLevel, Target, DeltaTime, Target > BreathLevel ? 1.2f : 0.25f);

    if (BreathComponent)
    {
        BreathComponent->SetVolumeMultiplier(FMath::Max(0.01f, BreathVolume * BreathLevel));
        BreathComponent->SetPitchMultiplier(0.92f + 0.2f * BreathLevel);
    }

    // Panting gives you away to anything close
    BreathNoiseTimer -= DeltaTime;
    if (BreathLevel > 0.45f && BreathNoiseTimer <= 0.f)
    {
        BreathNoiseTimer = 1.f;
        AEnemy::ReportNoise(this, GetPawnViewLocation(), BreathNoiseRange * BreathLevel, this);
    }

    // Bodycam mic auto-gain recovering after a gunshot
    AmbienceDuck = FMath::FInterpTo(AmbienceDuck, 1.f, DeltaTime, 1.5f);
    if (AmbienceComponent)
    {
        AmbienceComponent->SetVolumeMultiplier(AmbienceVolume * AmbienceDuck);
    }
}

// ------------------------------------------------------------------ fear

void AMyFPSPawn::UpdateFear(float DeltaTime)
{
    // What's scaring you, checked a few times a second
    FearScanTimer -= DeltaTime;
    if (FearScanTimer <= 0.f)
    {
        FearScanTimer = 0.25f;

        // Something hunting you nearby is the worst; something searching close by, or just
        // wandering right next to you in the dark, less so.
        float Threat = 0.f;
        const FVector Me = GetActorLocation();
        for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
        {
            const AEnemy* Enemy = *It;
            if (!Enemy || Enemy->IsDead())
            {
                continue;
            }
            const float Dist = static_cast<float>(FVector::Dist(Enemy->GetActorLocation(), Me));
            if (Dist > FearEnemyRange)
            {
                continue;
            }
            const float Close = 1.f - Dist / FMath::Max(FearEnemyRange, 1.f);
            float Weight = 0.f;
            switch (Enemy->GetState())
            {
            case EAIState::Chase:
            case EAIState::Attack: Weight = 0.35f + 0.65f * Close; break;
            case EAIState::Search: Weight = 0.45f * Close; break;
            default:               Weight = 0.2f * Close * Close; break;
            }
            Threat = FMath::Max(Threat, Weight);
        }

        const float Presence = BodycamLens ? BodycamLens->GetInterference() * 0.9f : 0.f;   // the Gaunt's pull
        const float Dark = Visibility < 0.25f ? 0.15f : 0.f;
        const float Hurt = 1.f - CurrentHealth / FMath::Max(MaxHealth, 1.f);
        FearTarget = bIsDead ? 0.f : FMath::Clamp(FMath::Max(Threat, Presence) + Dark + Hurt * 0.35f, 0.f, 1.f);
    }

    // Builds fast, fades slowly - your heart keeps going for a while after a scare
    if (FearTarget > Fear)
        Fear = FMath::Min(FearTarget, Fear + FearRiseRate * DeltaTime);
    else
        Fear = FMath::Max(FearTarget, Fear - FearFallRate * DeltaTime);

    if (HeartbeatComponent)
    {
        const float Audible = FMath::SmoothStep(0.15f, 0.7f, Fear);
        HeartbeatComponent->SetVolumeMultiplier(FMath::Max(0.01f, HeartbeatVolume * Audible));
        HeartbeatComponent->SetPitchMultiplier(1.f + 0.6f * Fear);   // ~66 bpm calm -> ~105 bpm terrified
    }
    if (BodycamLens)
    {
        BodycamLens->SetStress(Fear);
    }
    FearWobbleTime += DeltaTime * (5.f + 7.f * Fear);
}

// ------------------------------------------------------------------ arms

void AMyFPSPawn::SetupArms()
{
    // Preferred: the IK-driven Containment Operator arms (grip the gun and the melee weapons)
    if (OperatorArms)
    {
        OperatorArms->SetSources(CurrentGun, Melee);
        bUsingOperatorArms = OperatorArms->InitOperatorArms();
    }
    if (bUsingOperatorArms)
    {
        if (ArmsMesh)
        {
            ArmsMesh->SetVisibility(false);
            ArmsMesh->SetComponentTickEnabled(false);
        }
        return;
    }

    // Fallback: the old Mixamo arms, glued to the gun mesh
    if (!ArmsMesh) return;

    if (USkeletalMesh* MeshAsset = ArmsMeshAsset.LoadSynchronous())
    {
        ArmsMesh->SetSkeletalMeshAsset(MeshAsset);
    }

    // Child of the gun mesh: ApplyFirstPersonOffset moves the gun, the arms come along
    if (CurrentGun && CurrentGun->GetGunMesh())
    {
        ArmsMesh->AttachToComponent(CurrentGun->GetGunMesh(), FAttachmentTransformRules::KeepRelativeTransform);
    }

    ArmsMesh->SetRelativeLocation(ArmsOffset);
    ArmsMesh->SetRelativeRotation(ArmsRotation);
    ArmsMesh->SetRelativeScale3D(FVector(ArmsScale));

    ArmsAnimState = -1;
    UpdateArmsAnimation();
}

void AMyFPSPawn::UpdateArmsAnimation()
{
    if (bUsingOperatorArms) return;   // operator arms pose themselves
    if (!ArmsMesh || !ArmsMesh->GetSkeletalMeshAsset()) return;

    const int32 Want = (!MovementInput.IsNearlyZero() && bIsOnGround) ? 1 : 0;
    if (Want == ArmsAnimState) return;
    ArmsAnimState = Want;

    UAnimationAsset* Anim = (Want == 1) ? ArmsWalkAnim.LoadSynchronous() : ArmsIdleAnim.LoadSynchronous();
    if (!Anim) Anim = ArmsWalkAnim.LoadSynchronous();   // pistol pose beats a T-pose
    if (Anim)
    {
        ArmsMesh->PlayAnimation(Anim, true);
    }
}

// ------------------------------------------------------------------ health

float AMyFPSPawn::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (bIsDead) return 0.f;   // no further damage once dead

    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    CurrentHealth -= DamageAmount;
    CurrentHealth = FMath::Max(0.f, CurrentHealth);

    if (USoundBase* S = HurtSound.LoadSynchronous())
    {
        UGameplayStatics::PlaySound2D(this, S, 1.f, FMath::FRandRange(0.9f, 1.05f));
    }
    LandingShakeImpulse = FMath::Max(LandingShakeImpulse, 4.f);   // flinch
    AEnemy::ReportNoise(this, GetPawnViewLocation(), 600.f, this);   // a grunt of pain

    if (CurrentHealth <= 0.f)
    {
        OnDeath();
    }

    return Applied;
}

void AMyFPSPawn::OnDeath()
{
    if (bIsDead) return;
    bIsDead = true;
    DeathTime = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Log, TEXT("Player has died"));

    bIsRunning = false;
    bIsAiming = false;
    if (Melee) Melee->StopGuard();
    if (OperatorArms) OperatorArms->SetVisibility(false);

    // HUD reads bIsDead to draw the death screen; we reload the level after a delay
    GetWorldTimerManager().SetTimer(TimerHandle_Respawn, this, &AMyFPSPawn::Respawn, RespawnDelay, false);
}

void AMyFPSPawn::Respawn()
{
    // Reloading the level also fires EndPlay on smart enemies, which saves the Q-table
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

// ------------------------------------------------------------------ weapon slots

bool AMyFPSPawn::IsMeleeEquipped() const
{
    return Melee && Melee->IsMeleeActive();
}

FString AMyFPSPawn::GetMeleeWeaponName() const
{
    if (Melee && Melee->GetWeapon())
    {
        return Melee->GetWeapon()->GetDisplayName();
    }
    return FString();
}

void AMyFPSPawn::EquipGun()
{
    if (bIsDead) return;
    if (Melee) Melee->SetMeleeActive(false);
    if (CurrentGun) CurrentGun->SetGunHidden(false);
}

void AMyFPSPawn::EquipMelee()
{
    if (bIsDead) return;

    // Already holding a melee weapon? Pressing 2 again swaps to the next one.
    if (IsMeleeEquipped())
    {
        Melee->CycleWeapon();
        return;
    }

    bIsAiming = false;
    if (CurrentGun) CurrentGun->SetGunHidden(true);
    if (Melee) Melee->SetMeleeActive(true);
}

// ------------------------------------------------------------------ tick

float AMyFPSPawn::LeanClearance(float WantedOffset) const
{
    if (FMath::Abs(WantedOffset) < 0.5f)
    {
        return WantedOffset;
    }
    UWorld* W = GetWorld();
    if (!W)
    {
        return WantedOffset;
    }
    const FVector Head = GetActorLocation() + FVector(0.f, 0.f, CameraBaseZ);
    const FVector Side = GetActorRightVector() * WantedOffset;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LeanProbe), false, this);
    if (CurrentGun) Params.AddIgnoredActor(CurrentGun);
    if (Melee && Melee->GetWeapon()) Params.AddIgnoredActor(Melee->GetWeapon());

    FHitResult Hit;
    if (W->SweepSingleByChannel(Hit, Head, Head + Side, FQuat::Identity, ECC_Camera,
            FCollisionShape::MakeSphere(LeanProbeRadius), Params))
    {
        return FMath::Sign(WantedOffset) * FMath::Max(0.f, static_cast<float>(Hit.Distance) - 1.f);
    }
    return WantedOffset;
}

void AMyFPSPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bIsDead)
    {
        MovementInput = FVector::ZeroVector;
        CurrentTurnInput = 0.f;
        CurrentLookInput = 0.f;
    }

    CurrentSpeed = FMath::FInterpTo(CurrentSpeed, DesiredSpeed, DeltaTime, SpeedInterp);

    // ---- Walking with momentum: you speed up and slow down over a few tenths of a second,
    //      and keep your momentum in the air.
    {
        const FVector InputDir = GetActorForwardVector() * MovementInput.X + GetActorRightVector() * MovementInput.Y;
        const float InputMag = FMath::Min(static_cast<float>(InputDir.Size()), 1.f);
        const float AimScale = FMath::Lerp(1.f, AimMoveScale, AimAlpha);
        const FVector WishVel = InputDir.GetSafeNormal() * CurrentSpeed * InputMag * AimScale;

        float Rate = AirControl;
        if (bIsOnGround)
        {
            Rate = WishVel.SizeSquared() >= PlanarVelocity.SizeSquared() ? GroundAcceleration : GroundBraking;
        }
        PlanarVelocity = FMath::VInterpTo(PlanarVelocity, WishVel, DeltaTime, Rate);
        PlanarVelocity.Z = 0.f;

        if (!PlanarVelocity.IsNearlyZero(1.f))
        {
            const FVector Delta = PlanarVelocity * DeltaTime;
            FHitResult Hit;
            AddActorWorldOffset(Delta, true, &Hit);

            if (Hit.bBlockingHit)
            {
                // Slide along the wall and lose the speed going into it
                FVector WallNormal = Hit.Normal;
                WallNormal.Z = 0.f;
                WallNormal = WallNormal.GetSafeNormal();
                const FVector Slide = FVector::VectorPlaneProject(Delta * (1.f - Hit.Time), WallNormal);
                AddActorWorldOffset(Slide, true);
                if (!WallNormal.IsNearlyZero())
                {
                    PlanarVelocity = FVector::VectorPlaneProject(PlanarVelocity, WallNormal);
                }
            }
        }
        else
        {
            PlanarVelocity = FVector::ZeroVector;
        }
    }

    // ---- Gravity, landing, fall damage
    bWasOnGround = bIsOnGround;
    Velocity.Z += Gravity * DeltaTime;
    Velocity.Z = FMath::Max(Velocity.Z, TerminalVelocity);

    FHitResult Hit;
    AddActorWorldOffset(FVector(0.f, 0.f, Velocity.Z * DeltaTime), true, &Hit);

    if (Hit.IsValidBlockingHit())
    {
        if (Velocity.Z < 0.f)
        {
            const float ImpactSpeed = -Velocity.Z;
            bIsOnGround = true;
            bIsJumping  = false;

            if (!bWasOnGround && ImpactSpeed > 150.f)
            {
                const float Hardness = FMath::Clamp(ImpactSpeed / 600.f, 0.3f, 2.f);
                if (bEnableCameraShake)
                    LandingShakeImpulse = LandingShakeAmountDeg * Hardness;
                PlayFootstep(FMath::Min(1.2f * Hardness, 1.6f), true);   // landing thud

                if (ImpactSpeed > SafeFallSpeed)
                {
                    // ~4 m hurts, ~15 m kills; a hard landing knocks the wind out of you
                    const float Damage = (ImpactSpeed - SafeFallSpeed) / FMath::Max(LethalFallSpeed - SafeFallSpeed, 1.f) * MaxHealth;
                    PlanarVelocity *= 0.25f;
                    Stamina = FMath::Max(0.f, Stamina - 25.f);
                    TimeSinceStaminaUse = 0.f;
                    UGameplayStatics::ApplyDamage(this, Damage, GetController(), this, UDamageType::StaticClass());
                }
            }
        }
        Velocity.Z = 0.f;
    }
    else
    {
        bIsOnGround = false;
    }

    if (bIsSliding)
    {
        SlideTimer -= DeltaTime;
        DesiredSpeed = FMath::FInterpTo(DesiredSpeed, WalkSpeed, DeltaTime, 3.f);
        TimeSinceStaminaUse = 0.f;
        if (SlideTimer <= 0.f)
            StopSlide();
    }

    const bool bSprinting = bIsRunning && !bIsCrouching && !bIsSliding
        && MovementInput.SizeSquared() > 0.01f && bIsOnGround;
    const bool bDraining = bSprinting || bIsSliding;

    if (bDraining)
    {
        float Drain = 0.f;
        if (bSprinting) Drain += SprintDrainRate;
        if (bIsSliding)  Drain += SlideDrainRate;
        Stamina = FMath::Max(0.f, Stamina - Drain * DeltaTime);
        TimeSinceStaminaUse = 0.f;

        if (Stamina <= 0.f)
        {
            bOutOfStamina = true;
            if (bIsSliding) StopSlide();
            bIsRunning = false;
            DesiredSpeed = WalkSpeed;
        }
    }
    else
    {
        TimeSinceStaminaUse += DeltaTime;
        if (TimeSinceStaminaUse >= RegenDelay)
        {
            Stamina = FMath::Min(MaxStamina, Stamina + RegenRate * DeltaTime);
            if (bOutOfStamina && Stamina >= MinSprintStamina * 1.2f)
                bOutOfStamina = false;
        }
    }

    const float CurrentHH = CapsuleComponent->GetUnscaledCapsuleHalfHeight();
    const float NewHH = FMath::FInterpTo(CurrentHH, TargetCapsuleHalfHeight, DeltaTime, CrouchInterpSpeed);
    const float DeltaHH = NewHH - CurrentHH;
    if (!FMath::IsNearlyZero(DeltaHH))
    {
        CapsuleComponent->SetCapsuleHalfHeight(NewHH, true);
        AddActorWorldOffset(FVector(0.f, 0.f, DeltaHH), true);
    }

    // ---- Head bob: one bob per footstep, steps paced by the distance actually walked
    const float PlanarSpeed = static_cast<float>(PlanarVelocity.Size());
    const bool bStepping = bEnableHeadBob && bIsOnGround && !bIsSliding && PlanarSpeed > 25.f;
    float WantBob = 0.f;
    if (bStepping)
    {
        const float Run01 = FMath::Clamp((PlanarSpeed - WalkSpeed) / FMath::Max(RunSpeed - WalkSpeed, 1.f), 0.f, 1.f);
        const float StepLength = bIsCrouching ? CrouchStepLength : FMath::Lerp(WalkStepLength, RunStepLength, Run01);
        HeadBobTimer += 2.f * UE_PI * PlanarSpeed * DeltaTime / FMath::Max(StepLength, 10.f);
        WantBob = FMath::Lerp(HeadBobAmplitude, HeadBobRunAmplitude, Run01)
            * FMath::Clamp(PlanarSpeed / FMath::Max(WalkSpeed, 1.f), 0.f, 1.f)
            * (bIsCrouching ? 0.6f : 1.f);
    }
    BobAmount = FMath::FInterpTo(BobAmount, WantBob, DeltaTime, 6.f);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        const FRotator TargetRot = PC->GetControlRotation();
        SmoothedCameraRotation = FMath::RInterpTo(SmoothedCameraRotation, TargetRot, DeltaTime, CameraSmoothingSpeed);
    }

    float TurnRoll = 0.f;
    float AngularSpeed = 0.f;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        const float CurrentYaw = PC->GetControlRotation().Yaw;
        const float CurrentPitch = PC->GetControlRotation().Pitch;
        const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, PreviousControlYaw);
        const float DeltaPitch = FMath::FindDeltaAngleDegrees(CurrentPitch, PreviousControlPitch);
        const float YawSpeed = DeltaYaw / FMath::Max(DeltaTime, 1e-6f);
        AngularSpeed = FMath::Sqrt(FMath::Square(DeltaYaw) + FMath::Square(DeltaPitch)) / FMath::Max(DeltaTime, 1e-6f);
        PreviousControlYaw = CurrentYaw;
        PreviousControlPitch = CurrentPitch;
        const float Normalized = FMath::Clamp(YawSpeed / YawSpeedForFullTilt, -1.f, 1.f);
        TurnRoll = Normalized * TurnRollMax;
    }
    const float StrafeRoll = MovementInput.Y * StrafeRollMax;
    TargetRoll  = TurnRoll + StrafeRoll;
    CurrentRoll = FMath::FInterpTo(CurrentRoll, TargetRoll, DeltaTime, RollInterpSpeed);

    const float WantSlidePitch = bIsSliding ? SlideTiltPitch : 0.f;
    CurrentSlidePitch = FMath::FInterpTo(CurrentSlidePitch, WantSlidePitch, DeltaTime, SlideTiltInterpSpeed);

    // Head tilt
    if (!bIsSliding)
    {
        if      (bTiltLeftHeld  && !bTiltRightHeld) TiltTarget = -TiltMaxAngle;
        else if (bTiltRightHeld && !bTiltLeftHeld)  TiltTarget =  TiltMaxAngle;
        else                                         TiltTarget =  0.f;
    }
    else
    {
        TiltTarget = 0.f;
    }
    TiltCurrent = FMath::FInterpTo(TiltCurrent, TiltTarget, DeltaTime, TiltInterpSpeed);

    float ShakePitch = 0.f, ShakeYaw = 0.f, ShakeRoll = 0.f;
    float JitterX = 0.f, JitterY = 0.f;
    if (bEnableCameraShake)
    {
        const float MoveAlpha  = FMath::Clamp(PlanarSpeed / FMath::Max(WalkSpeed, 1.f), 0.f, 1.f);
        const float SpeedAlpha = FMath::Clamp(PlanarSpeed / FMath::Max(RunSpeed, 1.f), 0.f, 1.f);
        const float ShakeAlpha = MoveAlpha * (0.5f + 0.5f * SpeedAlpha);

        ShakeNoiseTime += DeltaTime * RunShakeFrequency * (0.35f + 0.65f * ShakeAlpha);
        const float n1 = FMath::PerlinNoise1D(ShakeNoiseTime);
        const float n2 = FMath::PerlinNoise1D(ShakeNoiseTime + 37.1f);
        const float n3 = FMath::PerlinNoise1D(ShakeNoiseTime + 81.7f);
        const float Amp = RunShakeAmplitudeDeg * ShakeAlpha;

        ShakePitch += n1 * Amp * 1.0f;
        ShakeYaw   += n2 * Amp * 0.6f;
        ShakeRoll  += n3 * Amp * 0.8f;

        if (LandingShakeImpulse > 0.f)
        {
            ShakePitch -= LandingShakeImpulse;
            LandingShakeImpulse = FMath::FInterpTo(LandingShakeImpulse, 0.f, DeltaTime, ShakeDecaySpeed);
        }

        JitterX = n3 * PosJitterAmplitude * 0.5f;
        JitterY = n2 * PosJitterAmplitude * 0.5f;
    }

    if (bIsSliding || bSprinting) LeanTarget = 0.f;
    else LeanTarget = (bLeanLeftHeld == bLeanRightHeld) ? 0.f : (bLeanRightHeld ? 1.f : -1.f);
    LeanAmount = FMath::FInterpTo(LeanAmount, LeanTarget, DeltaTime, LeanInterpSpeed);

    // ---- Camera position, rebuilt every frame: crouch height + footstep bob + jitter + lean
    //      (lean stops short of walls so you can't see through them)
    CameraBaseZ = FMath::FInterpTo(CameraBaseZ, TargetCameraZ, DeltaTime, CameraLerpSpeed);
    {
        const float LeanY = LeanClearance(LeanAmount * LeanOffset);
        FVector CamLoc(JitterX, JitterY + LeanY, CameraBaseZ - FMath::Abs(LeanAmount) * 3.f);
        CamLoc.Z += FMath::Sin(HeadBobTimer) * BobAmount;
        CamLoc.Y += FMath::Sin(HeadBobTimer * 0.5f) * BobAmount * 0.6f;   // weight shifting foot to foot
        CameraComponent->SetRelativeLocation(CamLoc);
    }

    RecoilCurrent = FMath::RInterpTo(RecoilCurrent, FRotator::ZeroRotator, DeltaTime, RecoilRecoverSpeed);
    FOVAdd = FMath::FInterpTo(FOVAdd, 0.f, DeltaTime, FOVRecoverSpeed);

    FRotator FinalRot = SmoothedCameraRotation;
    FinalRot.Roll  += CurrentRoll + (LeanAmount * LeanRoll) + ShakeRoll + TiltCurrent;
    FinalRot.Pitch += CurrentSlidePitch + ShakePitch + RecoilCurrent.Pitch;
    FinalRot.Yaw   += ShakeYaw + (LeanAmount * LeanYaw) + RecoilCurrent.Yaw;

    CameraComponent->SetWorldRotation(FinalRot);

    // ---- Aim down sights
    const bool bCanAim = bIsAiming && !IsMeleeEquipped() && !bSprinting && !bIsSliding;
    AimAlpha = FMath::FInterpTo(AimAlpha, bCanAim ? 1.f : 0.f, DeltaTime, AimInSpeed);

    float TargetFOV = BaseFOV;
    if (bIsSliding) TargetFOV = BaseFOV + SlideFOVAdd;
    else if (bIsRunning && PlanarSpeed > WalkSpeed * 1.1f) TargetFOV = BaseFOV + SprintFOVAdd;
    TargetFOV = FMath::Max(10.f, TargetFOV - AimFOVAdd * AimAlpha);
    TargetFOV += FOVAdd;

    const float NewFOV = FMath::FInterpTo(CameraComponent->FieldOfView, TargetFOV, DeltaTime, FOVInterpSpeed);
    CameraComponent->SetFieldOfView(NewFOV);

    if (BodycamLens)
    {
        BodycamLens->SetCameraMotion(AngularSpeed);
    }

    UpdateGunSway(DeltaTime);
    if (CurrentGun)
    {
        CurrentGun->SetHandling(AimAlpha,
            PlanarSpeed / FMath::Max(RunSpeed, 1.f),
            !bIsOnGround,
            FMath::Max(1.f - Stamina / FMath::Max(MaxStamina, 1.f), Fear * 0.7f),   // shaking hands count too
            bIsCrouching);
        CurrentGun->ApplyFirstPersonOffset(SwayCurrentLoc, SwayCurrentRot);
    }

    UpdateArmsAnimation();
    UpdateFootsteps();
    UpdateFear(DeltaTime);
    UpdateBreathing(DeltaTime);
    UpdateVisibility(DeltaTime);
}

void AMyFPSPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis("MoveForward", this, &AMyFPSPawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &AMyFPSPawn::MoveRight);
    PlayerInputComponent->BindAxis("Turn",        this, &AMyFPSPawn::Turn);
    PlayerInputComponent->BindAxis("LookUp",      this, &AMyFPSPawn::LookUp);

    PlayerInputComponent->BindAction("Jump",   IE_Pressed,  this, &AMyFPSPawn::StartJump);
    PlayerInputComponent->BindAction("Jump",   IE_Released, this, &AMyFPSPawn::StopJump);
    PlayerInputComponent->BindAction("Run",    IE_Pressed,  this, &AMyFPSPawn::StartRun);
    PlayerInputComponent->BindAction("Run",    IE_Released, this, &AMyFPSPawn::StopRun);
    PlayerInputComponent->BindAction("Crouch", IE_Pressed,  this, &AMyFPSPawn::ToggleCrouch);
    PlayerInputComponent->BindAction("Fire",   IE_Pressed,  this, &AMyFPSPawn::OnFirePressed);
    PlayerInputComponent->BindAction("Reload", IE_Pressed,  this, &AMyFPSPawn::OnReloadPressed);
    PlayerInputComponent->BindAction("Aim",    IE_Pressed,  this, &AMyFPSPawn::OnAimPressed);
    PlayerInputComponent->BindAction("Aim",    IE_Released, this, &AMyFPSPawn::OnAimReleased);

    PlayerInputComponent->BindKey(EKeys::LeftMouseButton,  IE_Pressed,  this, &AMyFPSPawn::OnFirePressed);
    PlayerInputComponent->BindKey(EKeys::R,                IE_Pressed,  this, &AMyFPSPawn::OnReloadPressed);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed,  this, &AMyFPSPawn::OnAimPressed);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AMyFPSPawn::OnAimReleased);
    PlayerInputComponent->BindKey(EKeys::Q,                IE_Pressed,  this, &AMyFPSPawn::TiltLeftPressed);
    PlayerInputComponent->BindKey(EKeys::Q,                IE_Released, this, &AMyFPSPawn::TiltLeftReleased);
    PlayerInputComponent->BindKey(EKeys::E,                IE_Pressed,  this, &AMyFPSPawn::TiltRightPressed);
    PlayerInputComponent->BindKey(EKeys::E,                IE_Released, this, &AMyFPSPawn::TiltRightReleased);

    // Lean (Z/C)
    PlayerInputComponent->BindKey(EKeys::Z, IE_Pressed,  this, &AMyFPSPawn::LeanLeftPressed);
    PlayerInputComponent->BindKey(EKeys::Z, IE_Released, this, &AMyFPSPawn::LeanLeftReleased);
    PlayerInputComponent->BindKey(EKeys::C, IE_Pressed,  this, &AMyFPSPawn::LeanRightPressed);
    PlayerInputComponent->BindKey(EKeys::C, IE_Released, this, &AMyFPSPawn::LeanRightReleased);

    // Weapon slots
    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMyFPSPawn::EquipGun);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMyFPSPawn::EquipMelee);

    // Chest torch
    PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AMyFPSPawn::ToggleTorch);
}

void AMyFPSPawn::ToggleTorch()
{
    if (!bIsDead && Torch)
    {
        Torch->Toggle();
        AEnemy::ReportNoise(this, GetPawnViewLocation(), 150.f, this);   // the click
    }
}

void AMyFPSPawn::MoveForward(float Value) { MovementInput.X = bIsDead ? 0.f : FMath::Clamp(Value, -1.f, 1.f); }
void AMyFPSPawn::MoveRight(float Value)   { MovementInput.Y = bIsDead ? 0.f : FMath::Clamp(Value, -1.f, 1.f); }

void AMyFPSPawn::Turn(float Value)
{
    if (bIsDead) return;
    // While guarding with melee, most of the mouse steers the blade instead of the camera
    if (Melee) Value = Melee->FilterLook(FVector2D(Value, 0.f)).X;
    CurrentTurnInput = Value;
    AddControllerYawInput(Value);
}

void AMyFPSPawn::LookUp(float Value)
{
    if (bIsDead) return;
    // LookUp is mapped with Scale -1, so flip it for the blade (mouse up = blade up)
    if (Melee) Value = -Melee->FilterLook(FVector2D(0.f, -Value)).Y;
    CurrentLookInput = Value;
    AddControllerPitchInput(Value);
}

void AMyFPSPawn::TiltLeftPressed()   { bTiltLeftHeld  = true;  }
void AMyFPSPawn::TiltLeftReleased()  { bTiltLeftHeld  = false; }
void AMyFPSPawn::TiltRightPressed()  { bTiltRightHeld = true;  }
void AMyFPSPawn::TiltRightReleased() { bTiltRightHeld = false; }

void AMyFPSPawn::StartJump()
{
    if (bIsOnGround && Stamina >= JumpCost)
    {
        Velocity.Z = JumpVelocity;
        bIsOnGround = false;
        bIsJumping  = true;
        Stamina = FMath::Max(0.f, Stamina - JumpCost);
        TimeSinceStaminaUse = 0.f;
    }
}

void AMyFPSPawn::StopJump()
{
    if (bIsJumping && Velocity.Z > 0.f)
        Velocity.Z *= 0.5f;
}

void AMyFPSPawn::StartRun()
{
    if (!bIsCrouching && !bIsSliding && !bOutOfStamina && Stamina >= MinSprintStamina)
    {
        bIsRunning = true;
        DesiredSpeed = RunSpeed;
    }
}

void AMyFPSPawn::StopRun()
{
    bIsRunning = false;
    if (!bIsCrouching && !bIsSliding)
        DesiredSpeed = WalkSpeed;
}

void AMyFPSPawn::ToggleCrouch()
{
    if (bIsSliding) return;

    const bool bSprinting = bIsRunning && bIsOnGround && MovementInput.SizeSquared() > 0.01f;
    if (bSprinting && !bIsCrouching && Stamina >= MinSlideStamina && !bOutOfStamina)
    {
        StartSlide();
        return;
    }

    if (bIsCrouching)
    {
        if (!CanStandUp()) return;
        bIsCrouching = false;
        TargetCapsuleHalfHeight = StandingHalfHeight;
        TargetCameraZ = 60.f;
        DesiredSpeed = bIsRunning ? RunSpeed : WalkSpeed;
    }
    else
    {
        bIsCrouching = true;
        TargetCapsuleHalfHeight = CrouchingHalfHeight;
        TargetCameraZ = 40.f;
        DesiredSpeed = CrouchSpeed;
        bIsRunning = false;
    }
}

void AMyFPSPawn::StartSlide()
{
    if (bIsSliding) return;
    if (Stamina < MinSlideStamina || bOutOfStamina) return;

    bIsSliding = true;
    SlideTimer = SlideDuration;
    DesiredSpeed = SlideSpeedBoost;
    CurrentSpeed = FMath::Max(CurrentSpeed, SlideSpeedBoost);
    TargetCapsuleHalfHeight = CrouchingHalfHeight;
    TargetCameraZ = 40.f;

    Stamina = FMath::Max(0.f, Stamina - MinSlideStamina * 0.2f);
    TimeSinceStaminaUse = 0.f;
}

void AMyFPSPawn::StopSlide()
{
    bIsSliding = false;
    TargetCapsuleHalfHeight = StandingHalfHeight;
    TargetCameraZ = 60.f;

    if (!bIsCrouching && MovementInput.SizeSquared() > 0.01f && !bOutOfStamina && Stamina >= MinSprintStamina)
    {
        bIsRunning = true;
        DesiredSpeed = RunSpeed;
    }
    else
    {
        bIsRunning = false;
        DesiredSpeed = WalkSpeed;
    }
}

void AMyFPSPawn::LeanLeftPressed()   { bLeanLeftHeld  = true;  }
void AMyFPSPawn::LeanLeftReleased()  { bLeanLeftHeld  = false; }
void AMyFPSPawn::LeanRightPressed()  { bLeanRightHeld = true;  }
void AMyFPSPawn::LeanRightReleased() { bLeanRightHeld = false; }

bool AMyFPSPawn::CanStandUp() const
{
    UWorld* W = GetWorld();
    if (!W || !CapsuleComponent) return true;

    const float Radius    = CapsuleComponent->GetUnscaledCapsuleRadius();
    const float CurrentHH = CapsuleComponent->GetUnscaledCapsuleHalfHeight();
    const float TargetHH  = StandingHalfHeight;

    const FVector StartCenter = GetActorLocation();
    const FVector EndCenter   = StartCenter + FVector(0.f, 0.f, TargetHH - CurrentHH);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(CanStandUp), false, this);

    if (W->OverlapBlockingTestByChannel(EndCenter, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius, TargetHH), Params))
        return false;

    FHitResult Hit;
    if (W->SweepSingleByChannel(Hit, StartCenter, EndCenter, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius, CurrentHH), Params))
        return false;

    return true;
}

void AMyFPSPawn::OnFirePressed()
{
    // LMB is bound twice (action mapping + direct key) - only act once per frame
    if (LastFireFrame == GFrameCounter) return;
    LastFireFrame = GFrameCounter;

    if (bIsDead) return;

    if (IsMeleeEquipped())
    {
        Melee->DoThrust();
        return;
    }

    if (!CurrentGun) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    const FVector  EyeLoc = CameraComponent->GetComponentLocation();
    const FRotator EyeRot = PC->GetControlRotation();

    const bool bFired = CurrentGun->Fire(EyeLoc, EyeRot, PC, this);
    if (bFired)
    {
        const float Pitch = CurrentGun->GetLastShotRecoilPitch();
        const float Yaw = CurrentGun->GetLastShotRecoilYaw();
        AddRecoil(Pitch, Yaw);
        AddFOVKick(CurrentGun->GetWeaponFOVKick());

        // Part of the climb stays: rapid fire walks the muzzle up unless you pull it down
        FRotator Control = PC->GetControlRotation();
        Control.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Control.Pitch + Pitch * RecoilClimb), -89.f, 89.f);
        Control.Yaw += Yaw * RecoilClimb;
        PC->SetControlRotation(Control);

        if (BodycamLens)
        {
            BodycamLens->OnGunshot();
        }
        AmbienceDuck = 0.2f;   // the mic's auto-gain clamps down on the bang
        LandingShakeImpulse = FMath::Max(LandingShakeImpulse, 0.6f);
    }
}

void AMyFPSPawn::OnReloadPressed()
{
    if (!bIsDead && !IsMeleeEquipped() && CurrentGun) CurrentGun->StartReload();
}

void AMyFPSPawn::OnAimPressed()
{
    if (bIsDead) return;
    if (IsMeleeEquipped()) { Melee->StartGuard(); return; }   // RMB = guard / steer blade
    bIsAiming = true;
    if (bIsRunning)
    {
        StopRun();   // you can't sprint and aim
    }
}

void AMyFPSPawn::OnAimReleased()
{
    if (Melee) Melee->StopGuard();
    bIsAiming = false;
}

void AMyFPSPawn::AddRecoil(float PitchDegrees, float YawDegrees)
{
    // Positive pitch = up: the muzzle flips up and the view goes with it, then settles
    RecoilCurrent.Pitch += PitchDegrees;
    RecoilCurrent.Yaw   += YawDegrees;
}

void AMyFPSPawn::AddFOVKick(float Amount)
{
    FOVAdd += Amount;
}

void AMyFPSPawn::UpdateGunSway(float DeltaTime)
{
    const float InputX = CurrentTurnInput;
    const float InputY = CurrentLookInput;
    const float PlanarSpeed = static_cast<float>(PlanarVelocity.Size());
    const float Fatigue = 1.f - Stamina / FMath::Max(MaxStamina, 1.f);
    const float AimScale = FMath::Lerp(1.f, AimSwayScale, AimAlpha);

    // Smooth mouse input into velocity so gun has inertia/lag
    SwayVelocity = FMath::VInterpTo(
        SwayVelocity,
        FVector(0.f, InputX, InputY),
        DeltaTime,
        SwayVelocitySmoothing);

    // Position sway driven by smoothed velocity
    FVector DesiredPos = FVector(0.f,
        -SwayVelocity.Y * SwayPosScale,
        -SwayVelocity.Z * SwayPosScale * 0.8f);

    // Stride sway - gun rocks when walking, synced to the footsteps
    if (bIsOnGround && !bIsSliding && PlanarSpeed > 25.f)
    {
        const float MoveScale = FMath::Clamp(PlanarSpeed / FMath::Max(WalkSpeed, 1.f), 0.f, 1.f);
        const float RunScale  = bIsRunning ? 2.0f : 1.0f;
        const float StrideAmp = SwayStrideAmplitude * MoveScale * RunScale;

        DesiredPos.Y += FMath::Sin(HeadBobTimer * 0.5f) * StrideAmp;
        DesiredPos.Z += FMath::Abs(FMath::Sin(HeadBobTimer)) * StrideAmp * -0.4f;
        DesiredPos.X += FMath::Sin(HeadBobTimer * 0.5f + 1.f) * StrideAmp * 0.3f;
    }

    DesiredPos = DesiredPos.GetClampedToMaxSize(SwayMaxOffset) * AimScale;

    // Breathing: the sights rise and fall with each breath - faster and bigger when you're blown
    IdleSwayTime += DeltaTime * (1.f + 1.4f * Fatigue);
    const float BreathAmp = (1.f + 2.5f * Fatigue) * FMath::Lerp(1.f, 0.6f, AimAlpha);
    DesiredPos.Z += FMath::Sin(IdleSwayTime * 1.4f) * 1.0f * BreathAmp;
    DesiredPos.Y += FMath::Sin(IdleSwayTime * 0.9f + 2.1f) * 0.7f * BreathAmp;

    const bool bMouseMoving = (FMath::Abs(InputX) + FMath::Abs(InputY)) > 0.001f;
    const float InterpSpeed = bMouseMoving ? SwayFollowSpeed : SwayReturnSpeed;

    SwayTargetLoc  = DesiredPos;
    SwayCurrentLoc = FMath::VInterpTo(SwayCurrentLoc, SwayTargetLoc, DeltaTime, InterpSpeed);

    // Rotation sway driven by smoothed velocity
    FRotator DesiredRot;
    DesiredRot.Pitch = -SwayVelocity.Z * SwayRotScalePitch;
    DesiredRot.Yaw   =  SwayVelocity.Y * SwayRotScaleYaw;
    DesiredRot.Roll  = -SwayVelocity.Y * SwayRotScaleRoll;

    // Extra roll from strafing
    DesiredRot.Roll += MovementInput.Y * SwayStrideAmplitude * 1.2f;

    // Breathing tips the muzzle too (this is what moves the crosshair when aimed)
    DesiredRot.Pitch += FMath::Sin(IdleSwayTime * 1.4f + 0.6f) * 0.15f * BreathAmp;

    // Fear: a fine tremor in the hands that aiming only partly steadies
    if (Fear > 0.01f)
    {
        const float Shake = FearAimWobbleDeg * Fear * Fear * FMath::Lerp(1.f, 0.6f, AimAlpha) / FMath::Max(AimScale, 0.01f);
        DesiredRot.Pitch += FMath::PerlinNoise1D(FearWobbleTime) * Shake;
        DesiredRot.Yaw   += FMath::PerlinNoise1D(FearWobbleTime + 51.3f) * Shake;
    }

    SwayTargetRot  = DesiredRot * AimScale;
    SwayCurrentRot = FMath::RInterpTo(SwayCurrentRot, SwayTargetRot, DeltaTime, InterpSpeed);
}
