#include "MyFPSPawn.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "AGun.h"
#include "MeleeCombatComponent.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "CoreGlobals.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "MeleeWeapon.h"

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

    // Physics melee
    Melee = CreateDefaultSubobject<UMeleeCombatComponent>(TEXT("Melee"));

    // Generated audio
    auto Snd = [](const TCHAR* Name)
    {
        return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Generated/Audio/%s.%s"), Name, Name)));
    };
    FootstepSounds = { Snd(TEXT("SW_Footstep_Concrete_01")), Snd(TEXT("SW_Footstep_Concrete_02")),
                       Snd(TEXT("SW_Footstep_Concrete_03")), Snd(TEXT("SW_Footstep_Concrete_04")) };
    HurtSound      = Snd(TEXT("SW_Player_Hurt"));
    AmbienceSound  = Snd(TEXT("SW_Ambience_BodycamHiss_Loop"));

    AutoPossessPlayer = EAutoReceiveInput::Player0;
    bUseControllerRotationYaw = true;

    CurrentSpeed = WalkSpeed;
    DesiredSpeed = WalkSpeed;

    TargetCapsuleHalfHeight = StandingHalfHeight;
    TargetCameraZ = 60.f;

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
    }
    CameraComponent->SetFieldOfView(BaseFOV);

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
}

// ------------------------------------------------------------------ footsteps

void AMyFPSPawn::PlayFootstep(float Volume)
{
    if (FootstepSounds.Num() == 0) return;
    if (USoundBase* S = FootstepSounds[FMath::RandRange(0, FootstepSounds.Num() - 1)].LoadSynchronous())
    {
        const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, CapsuleComponent->GetScaledCapsuleHalfHeight());
        UGameplayStatics::PlaySoundAtLocation(this, S, Feet, Volume * FootstepVolume, FMath::FRandRange(0.92f, 1.08f));
    }
}

void AMyFPSPawn::UpdateFootsteps()
{
    // One step per headbob cycle, landing on the bottom of the bob (sin = -1 at 1.5 pi)
    if (!bEnableHeadBob || HeadBobTimer <= 0.f)
    {
        LastStepIndex = 0;
        return;
    }
    const int32 StepIndex = FMath::FloorToInt((HeadBobTimer + 0.5f * UE_PI) / (2.f * UE_PI));
    if (StepIndex != LastStepIndex)
    {
        LastStepIndex = StepIndex;
        float Vol = 0.7f;
        if (bIsRunning)   Vol = 1.f;
        if (bIsCrouching) Vol = 0.35f;
        PlayFootstep(Vol);
    }
}

// ------------------------------------------------------------------ arms

void AMyFPSPawn::SetupArms()
{
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

    if (!MovementInput.IsNearlyZero())
    {
        const FVector MoveDir = (GetActorForwardVector() * MovementInput.X
            + GetActorRightVector() * MovementInput.Y).GetSafeNormal();

        FVector Delta = MoveDir * CurrentSpeed * DeltaTime;

        FHitResult Hit;
        AddActorWorldOffset(Delta, true, &Hit);

        if (Hit.bBlockingHit)
        {
            const FVector Slide = FVector::VectorPlaneProject(Delta, Hit.Normal);
            AddActorWorldOffset(Slide, true);
        }
    }

    bWasOnGround = bIsOnGround;
    Velocity.Z += Gravity * DeltaTime;
    Velocity.Z = FMath::Max(Velocity.Z, TerminalVelocity);

    FHitResult Hit;
    AddActorWorldOffset(FVector(0.f, 0.f, Velocity.Z * DeltaTime), true, &Hit);

    if (Hit.IsValidBlockingHit())
    {
        if (Velocity.Z < 0.f)
        {
            bIsOnGround = true;
            bIsJumping  = false;

            if (!bWasOnGround && bEnableCameraShake)
                LandingShakeImpulse = LandingShakeAmountDeg;
            if (!bWasOnGround)
                PlayFootstep(1.2f);   // landing thud
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

    FVector CamRel = CameraComponent->GetRelativeLocation();
    CamRel.Z = FMath::FInterpTo(CamRel.Z, TargetCameraZ, DeltaTime, CameraLerpSpeed);
    CameraComponent->SetRelativeLocation(CamRel);

    if (bEnableHeadBob && bIsOnGround && !MovementInput.IsNearlyZero() && !bIsSliding)
    {
        HeadBobTimer += DeltaTime * HeadBobFrequency;
        const float BobOffset = FMath::Sin(HeadBobTimer) * HeadBobAmplitude;
        FVector BobLoc = CameraComponent->GetRelativeLocation();
        BobLoc.Z = CamRel.Z + BobOffset;
        CameraComponent->SetRelativeLocation(BobLoc);
    }
    else
    {
        HeadBobTimer = 0.f;
    }

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        const FRotator TargetRot = PC->GetControlRotation();
        SmoothedCameraRotation = FMath::RInterpTo(SmoothedCameraRotation, TargetRot, DeltaTime, CameraSmoothingSpeed);
    }

    float TurnRoll = 0.f;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        const float CurrentYaw = PC->GetControlRotation().Yaw;
        float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, PreviousControlYaw);
        const float YawSpeed = DeltaYaw / FMath::Max(DeltaTime, 1e-6f);
        PreviousControlYaw = CurrentYaw;
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
    if (bEnableCameraShake)
    {
        const float MoveAlpha  = FMath::Clamp(MovementInput.Size(), 0.f, 1.f);
        const float SpeedAlpha = FMath::Clamp(CurrentSpeed / RunSpeed, 0.f, 1.f);
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

        FVector CamRL = CameraComponent->GetRelativeLocation();
        CamRL += FVector(n3 * PosJitterAmplitude * 0.5f, n2 * PosJitterAmplitude * 0.5f, 0.f);
        CameraComponent->SetRelativeLocation(CamRL);
    }

    RecoilCurrent = FMath::RInterpTo(RecoilCurrent, FRotator::ZeroRotator, DeltaTime, RecoilRecoverSpeed);
    FOVAdd = FMath::FInterpTo(FOVAdd, 0.f, DeltaTime, FOVRecoverSpeed);

    if (bIsSliding) LeanTarget = 0.f;
    LeanAmount = FMath::FInterpTo(LeanAmount, LeanTarget, DeltaTime, LeanInterpSpeed);

    FRotator FinalRot = SmoothedCameraRotation;
    FinalRot.Roll  += CurrentRoll + (LeanAmount * LeanRoll) + ShakeRoll + TiltCurrent;
    FinalRot.Pitch += CurrentSlidePitch + ShakePitch + RecoilCurrent.Pitch;
    FinalRot.Yaw   += ShakeYaw + (LeanAmount * LeanYaw) + RecoilCurrent.Yaw;

    CameraComponent->SetWorldRotation(FinalRot);

    float TargetFOV = BaseFOV;
    if (bIsSliding) TargetFOV = BaseFOV + SlideFOVAdd;
    else if (bIsRunning && CurrentSpeed > WalkSpeed * 0.5f) TargetFOV = BaseFOV + SprintFOVAdd;
    if (bIsAiming) TargetFOV = FMath::Max(10.f, TargetFOV - AimFOVAdd);
    TargetFOV += FOVAdd;

    const float NewFOV = FMath::FInterpTo(CameraComponent->FieldOfView, TargetFOV, DeltaTime, FOVInterpSpeed);
    CameraComponent->SetFieldOfView(NewFOV);

    UpdateGunSway(DeltaTime);
    if (CurrentGun)
        CurrentGun->ApplyFirstPersonOffset(SwayCurrentLoc, SwayCurrentRot);

    UpdateArmsAnimation();
    UpdateFootsteps();
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

void AMyFPSPawn::LeanLeftPressed()   { bLeanLeftHeld  = true;  LeanTarget = bLeanRightHeld ? 0.f : -1.f; }
void AMyFPSPawn::LeanLeftReleased()  { bLeanLeftHeld  = false; LeanTarget = bLeanRightHeld ? +1.f :  0.f; }
void AMyFPSPawn::LeanRightPressed()  { bLeanRightHeld = true;  LeanTarget = bLeanLeftHeld  ? 0.f : +1.f; }
void AMyFPSPawn::LeanRightReleased() { bLeanRightHeld = false; LeanTarget = bLeanLeftHeld  ? -1.f : 0.f; }

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
        AddRecoil(CurrentGun->GetRecoilPitch(),
            FMath::FRandRange(-CurrentGun->GetRecoilYaw(), CurrentGun->GetRecoilYaw()));
        AddFOVKick(CurrentGun->GetWeaponFOVKick());
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
}

void AMyFPSPawn::OnAimReleased()
{
    if (Melee) Melee->StopGuard();
    bIsAiming = false;
}

void AMyFPSPawn::AddRecoil(float PitchDegrees, float YawDegrees)
{
    RecoilCurrent.Pitch -= PitchDegrees;
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

    // Stride sway - gun rocks when walking, synced to headbob
    if (bIsOnGround && !bIsSliding && !MovementInput.IsNearlyZero())
    {
        const float MoveScale = FMath::Clamp(MovementInput.Size(), 0.f, 1.f);
        const float RunScale  = bIsRunning ? 2.0f : 1.0f;
        const float StrideAmp = SwayStrideAmplitude * MoveScale * RunScale;

        DesiredPos.Y += FMath::Sin(HeadBobTimer * 0.5f) * StrideAmp;
        DesiredPos.Z += FMath::Abs(FMath::Sin(HeadBobTimer)) * StrideAmp * -0.4f;
        DesiredPos.X += FMath::Sin(HeadBobTimer * 0.5f + 1.f) * StrideAmp * 0.3f;
    }

    DesiredPos = DesiredPos.GetClampedToMaxSize(SwayMaxOffset);

    // Idle breathing
    IdleSwayTime += DeltaTime;
    DesiredPos.Z += FMath::Sin(IdleSwayTime * 1.4f) * 1.0f;
    DesiredPos.Y += FMath::Sin(IdleSwayTime * 0.9f + 2.1f) * 0.7f;

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

    SwayTargetRot  = DesiredRot;
    SwayCurrentRot = FMath::RInterpTo(SwayCurrentRot, SwayTargetRot, DeltaTime, InterpSpeed);
}
