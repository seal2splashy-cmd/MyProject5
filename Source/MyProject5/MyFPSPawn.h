#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MyFPSPawn.generated.h"

class UCapsuleComponent;
class UCameraComponent;
class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimationAsset;
class UMeleeCombatComponent;
class UOperatorArmsComponent;
class UBodycamLensComponent;
class UBodycamTorchComponent;
class UAudioComponent;
class USoundBase;
class AGun;
class AAHInteractable;

/**
 * The player. Manual movement (no CharacterMovement), stamina, bodycam camera, FP arms, weapons.
 * Nov 2026 realism pass (Docs/REALISM.md): real-world walking/sprinting speeds with momentum,
 * footstep-paced head bob, per-surface footsteps that enemies hear, fall damage, breathing that
 * gets heavier as you tire, lean that stops at walls, ADS that raises the gun, and a visibility
 * value (light you're standing in + your torch) that enemies' eyes use.
 */
UCLASS()
class MYPROJECT5_API AMyFPSPawn : public APawn
{
    GENERATED_BODY()

public:
    AMyFPSPawn();

    AGun* GetCurrentGun() const { return CurrentGun; }
    UBodycamTorchComponent* GetTorch() const { return Torch; }
    UBodycamLensComponent* GetBodycamLens() const { return BodycamLens; }

    // HUD accessors
    float GetHealth() const     { return CurrentHealth; }
    float GetMaxHealth() const  { return MaxHealth; }
    float GetStamina() const    { return Stamina; }
    float GetMaxStamina() const { return MaxStamina; }
    bool  IsDead() const        { return bIsDead; }
    float GetDeathTime() const  { return DeathTime; }
    bool  IsMeleeEquipped() const;
    FString GetMeleeWeaponName() const;

    /** How easy you are to see: 0 = in the dark, torch off; 1 = lit up. Crouching lowers it. */
    float GetVisibility01() const { return Visibility; }
    /** True if the torch is on and Point is inside its beam (the beam itself gives you away). */
    bool IsTorchShiningAt(const FVector& Point) const;

    // ---- Interaction (F): doors, pickups
    /** "[F] ..." text for the HUD, empty when you're not looking at anything usable. */
    FString GetInteractPrompt() const;
    float GetPlanarSpeed() const { return static_cast<float>(PlanarVelocity.Size()); }
    float GetWalkSpeed() const { return WalkSpeed; }
    /** True while a bandage is still working. */
    bool IsHealing() const { return PendingHeal > 0.f; }

    bool CanTakeMagazine() const;
    bool TakeMagazine(int32 Rounds);
    bool CanUseBandage() const;
    bool UseBandage(float Amount);
    bool CanTakeBattery() const;
    bool TakeBattery(float Amount);

    /** The eyes are the camera (so leaning and crouching move where enemies can see you). */
    virtual FVector GetPawnViewLocation() const override;

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
    // Health / death
    void OnDeath();
    void Respawn();

    UPROPERTY(EditAnywhere, Category = "Health")
    float MaxHealth = 100.f;

    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    float CurrentHealth = 100.f;

    UPROPERTY(EditAnywhere, Category = "Health")
    float RespawnDelay = 4.f;

    /** Landing faster than this hurts (~4 m drop)... */
    UPROPERTY(EditAnywhere, Category = "Health|Falling")
    float SafeFallSpeed = 900.f;

    /** ...and this kills (~15 m drop). */
    UPROPERTY(EditAnywhere, Category = "Health|Falling")
    float LethalFallSpeed = 1750.f;

    bool  bIsDead = false;
    float DeathTime = -1.f;
    FTimerHandle TimerHandle_Respawn;

    // First-person arms.
    // Preferred: OperatorArms (IK onto the gun / melee grip). Fallback if that mesh isn't
    // imported: the old Mixamo ArmsMesh glued to the gun mesh.
    void SetupArms();
    void UpdateArmsAnimation();

    UPROPERTY(VisibleAnywhere, Category = "Arms")
    UOperatorArmsComponent* OperatorArms;

    bool bUsingOperatorArms = false;

    UPROPERTY(VisibleAnywhere, Category = "Arms")
    USkeletalMeshComponent* ArmsMesh;

    UPROPERTY(EditAnywhere, Category = "Arms")
    TSoftObjectPtr<USkeletalMesh> ArmsMeshAsset;

    UPROPERTY(EditAnywhere, Category = "Arms")
    TSoftObjectPtr<UAnimationAsset> ArmsIdleAnim;

    UPROPERTY(EditAnywhere, Category = "Arms")
    TSoftObjectPtr<UAnimationAsset> ArmsWalkAnim;

    /** Tweak in the editor until the hands sit on the grip. */
    UPROPERTY(EditAnywhere, Category = "Arms")
    FVector ArmsOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Arms")
    FRotator ArmsRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category = "Arms")
    float ArmsScale = 1.f;

    int32 ArmsAnimState = -1; // 0 idle, 1 walk

    // Melee (key 2) / gun (key 1)
    void EquipGun();
    void EquipMelee();

    UPROPERTY(VisibleAnywhere, Category = "Melee")
    UMeleeCombatComponent* Melee;

    // Bodycam look: lens post-process on the camera + chest torch (T)
    UPROPERTY(VisibleAnywhere, Category = "Bodycam")
    UBodycamLensComponent* BodycamLens;

    UPROPERTY(VisibleAnywhere, Category = "Bodycam")
    UBodycamTorchComponent* Torch;

    void ToggleTorch();

    // ---- Interaction
    void UpdateFocus();
    void Interact();

    /** What the camera is pointed at, if it's usable and close enough. */
    TWeakObjectPtr<AAHInteractable> Focused;

    /** Health per second while a bandage works. */
    UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = 1))
    float BandageHealRate = 6.f;

    float PendingHeal = 0.f;

    uint64 LastFireFrame = 0;

    // ---- Visibility (what enemies' eyes use) ----
    void UpdateVisibility(float DeltaTime);

    /** How visible you are with no light on you (moonlight, glow from screens...). */
    UPROPERTY(EditAnywhere, Category = "Stealth", meta = (ClampMin = 0, ClampMax = 1))
    float AmbientVisibility = 0.12f;

    /** Added while your torch is on (the glow around you; the beam itself is IsTorchShiningAt). */
    UPROPERTY(EditAnywhere, Category = "Stealth", meta = (ClampMin = 0, ClampMax = 1))
    float TorchVisibility = 0.3f;

    /** How much standing right under a lamp counts. */
    UPROPERTY(EditAnywhere, Category = "Stealth", meta = (ClampMin = 0, ClampMax = 2))
    float LampVisibility = 0.9f;

    float Visibility = 1.f;
    float VisibilityTimer = 0.f;

    // ---- Noise you make (cm enemies hear it from; see AEnemy::ReportNoise) ----
    UPROPERTY(EditAnywhere, Category = "Stealth|Noise")
    float WalkNoiseRange = 450.f;

    UPROPERTY(EditAnywhere, Category = "Stealth|Noise")
    float SprintNoiseRange = 1100.f;

    /** 0 = sneaking is silent. */
    UPROPERTY(EditAnywhere, Category = "Stealth|Noise")
    float CrouchNoiseRange = 0.f;

    UPROPERTY(EditAnywhere, Category = "Stealth|Noise")
    float LandNoiseRange = 900.f;

    /** Panting when out of breath carries this far at its worst. */
    UPROPERTY(EditAnywhere, Category = "Stealth|Noise")
    float BreathNoiseRange = 350.f;

    // ---- Fear: being hunted, the Gaunt close, darkness, being badly hurt.
    //      Heart pounds, hands shake (aim wobble + spread), breathing quickens, the picture narrows.
    void UpdateFear(float DeltaTime);

    /** A hunting enemy closer than this starts to scare you (cm). */
    UPROPERTY(EditAnywhere, Category = "Fear")
    float FearEnemyRange = 2500.f;

    /** How fast fear builds / fades (per second). */
    UPROPERTY(EditAnywhere, Category = "Fear")
    float FearRiseRate = 1.2f;

    UPROPERTY(EditAnywhere, Category = "Fear")
    float FearFallRate = 0.12f;

    /** Shaking hands at full fear (degrees of extra aim wobble). */
    UPROPERTY(EditAnywhere, Category = "Fear")
    float FearAimWobbleDeg = 1.1f;

    UPROPERTY(EditAnywhere, Category = "Fear")
    TSoftObjectPtr<USoundBase> HeartbeatSound;

    UPROPERTY(EditAnywhere, Category = "Fear")
    float HeartbeatVolume = 0.9f;

    UPROPERTY(Transient)
    UAudioComponent* HeartbeatComponent = nullptr;

    float Fear = 0.f;           // 0..1, smoothed
    float FearTarget = 0.f;
    float FearScanTimer = 0.f;
    float FearWobbleTime = 0.f;

public:
    float GetFear01() const { return Fear; }

private:
    // Audio (generated by Tools/generate_assets.py; silently skipped if not imported)
    void UpdateFootsteps();
    void PlayFootstep(float Volume, bool bLanding = false);
    void UpdateBreathing(float DeltaTime);

    /** Fallback footsteps if the per-surface ones aren't imported. */
    UPROPERTY(EditAnywhere, Category = "Audio")
    TArray<TSoftObjectPtr<USoundBase>> FootstepSounds;

    UPROPERTY(EditAnywhere, Category = "Audio")
    TSoftObjectPtr<USoundBase> HurtSound;

    UPROPERTY(EditAnywhere, Category = "Audio")
    TSoftObjectPtr<USoundBase> AmbienceSound;

    /** Looping heavy breathing, faded in as you tire or get hurt. */
    UPROPERTY(EditAnywhere, Category = "Audio")
    TSoftObjectPtr<USoundBase> BreathSound;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float AmbienceVolume = 0.35f;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float FootstepVolume = 0.6f;

    UPROPERTY(EditAnywhere, Category = "Audio")
    float BreathVolume = 0.75f;

    UPROPERTY(Transient)
    UAudioComponent* AmbienceComponent = nullptr;

    UPROPERTY(Transient)
    UAudioComponent* BreathComponent = nullptr;

    /** The bodycam mic's auto-gain: a gunshot ducks the background hiss, which then creeps back. */
    float AmbienceDuck = 1.f;
    float BreathLevel = 0.f;
    float BreathNoiseTimer = 0.f;

    int32 LastStepIndex = 0;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);

    void StartJump();
    void StopJump();
    void StartRun();
    void StopRun();
    void ToggleCrouch();
    void StartSlide();
    void StopSlide();

    void LeanLeftPressed();
    void LeanLeftReleased();
    void LeanRightPressed();
    void LeanRightReleased();

    void TiltLeftPressed();
    void TiltLeftReleased();
    void TiltRightPressed();
    void TiltRightReleased();

    void OnFirePressed();
    void OnReloadPressed();
    void OnAimPressed();
    void OnAimReleased();

    bool CanStandUp() const;
    void UpdateGunSway(float DeltaTime);
    /** How far the camera can lean (cm, signed) before the head would go into a wall. */
    float LeanClearance(float WantedOffset) const;

    // Components
    UPROPERTY(VisibleAnywhere)
    UCapsuleComponent* CapsuleComponent;

    UPROPERTY(VisibleAnywhere)
    UCameraComponent* CameraComponent;

    FVector MovementInput;

    // Movement (cm/s): a brisk walk ~3 m/s, a hard sprint ~6.5 m/s - real-world feel but game pace
    UPROPERTY(EditAnywhere, Category = "Movement")
    float WalkSpeed = 300.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float RunSpeed = 650.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float CrouchSpeed = 165.f;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float SlideSpeedBoost = 780.f;

    /** Walking speed x this while aiming down sights. */
    UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = 0.1, ClampMax = 1))
    float AimMoveScale = 0.6f;

    /** How quickly you get up to speed on the ground (higher = snappier). */
    UPROPERTY(EditAnywhere, Category = "Movement")
    float GroundAcceleration = 9.f;

    /** How quickly you stop. */
    UPROPERTY(EditAnywhere, Category = "Movement")
    float GroundBraking = 11.f;

    /** Steering in the air (you mostly keep the momentum you jumped with). */
    UPROPERTY(EditAnywhere, Category = "Movement")
    float AirControl = 0.6f;

    /** Tallest ledge you walk straight up without jumping: stair risers, kerbs, door sills (cm). */
    UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = 0, ClampMax = 80))
    float MaxStepHeight = 40.f;

    /** Steepest ground you can walk on, as the floor normal's Z (0.71 = 45 degrees). Ramps up to
     *  this are walked up; steeper is a wall. Walking down slopes/stairs keeps you on them
     *  (no little falls every step). */
    UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = 0.1, ClampMax = 1))
    float WalkableFloorZ = 0.71f;

    /** Step up onto a ledge up to MaxStepHeight while moving by Remaining. False (and nothing moved) if it can't. */
    bool TryStepUp(const FVector& Remaining);

    float CurrentSpeed = 300.f;
    float DesiredSpeed = 300.f;
    FVector PlanarVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Movement")
    float SpeedInterp = 8.f;

    // Gravity & Jump
    FVector Velocity;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float Gravity = -980.f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float TerminalVelocity = -4000.f;

    UPROPERTY(EditAnywhere, Category = "Jump")
    float JumpVelocity = 350.f;

    bool bIsOnGround  = true;
    bool bWasOnGround = true;
    bool bIsJumping   = false;

    // Crouch
    bool bIsCrouching = false;

    UPROPERTY(EditAnywhere, Category = "Crouch")
    float StandingHalfHeight = 88.f;

    UPROPERTY(EditAnywhere, Category = "Crouch")
    float CrouchingHalfHeight = 44.f;

    UPROPERTY(EditAnywhere, Category = "Crouch")
    float CrouchInterpSpeed = 10.f;

    UPROPERTY(EditAnywhere, Category = "Crouch")
    float CameraLerpSpeed = 10.f;

    float TargetCapsuleHalfHeight = 88.f;
    float TargetCameraZ = 60.f;
    float CameraBaseZ = 60.f;

    // Slide
    bool bIsSliding = false;

    UPROPERTY(EditAnywhere, Category = "Slide")
    float SlideDuration = 0.7f;

    float SlideTimer = 0.f;

    // Stamina
    UPROPERTY(EditAnywhere, Category = "Stamina")
    float MaxStamina = 100.f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float SprintDrainRate = 22.f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float SlideDrainRate = 40.f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float JumpCost = 15.f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float RegenRate = 20.f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float RegenDelay = 0.7f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float MinSprintStamina = 10.f;

    UPROPERTY(EditAnywhere, Category = "Stamina")
    float MinSlideStamina = 20.f;

    float Stamina = 100.f;
    float TimeSinceStaminaUse = 0.f;
    bool  bOutOfStamina = false;
    bool  bIsRunning    = false;

    // Camera
    FRotator SmoothedCameraRotation;

    UPROPERTY(EditAnywhere, Category = "Camera")
    float CameraSmoothingSpeed = 8.f;

    float TargetRoll  = 0.f;
    float CurrentRoll = 0.f;

    /** Roll when turning fast. A chest-mounted camera barely rolls; 25 was a cartoon. */
    UPROPERTY(EditAnywhere, Category = "Camera|Roll")
    float TurnRollMax = 3.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Roll")
    float StrafeRollMax = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Camera|Roll")
    float RollInterpSpeed = 6.f;

    float CurrentTurnInput = 0.f;
    float CurrentLookInput = 0.f;
    float PreviousControlYaw = 0.f;
    float PreviousControlPitch = 0.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Roll")
    float YawSpeedForFullTilt = 200.f;

    // Headbob: one bob per footstep, footsteps paced by distance walked
    UPROPERTY(EditAnywhere, Category = "Camera|Headbob")
    bool bEnableHeadBob = true;

    /** Vertical bob walking (cm). */
    UPROPERTY(EditAnywhere, Category = "Camera|Headbob")
    float HeadBobAmplitude = 1.1f;

    /** Vertical bob sprinting (cm). */
    UPROPERTY(EditAnywhere, Category = "Camera|Headbob")
    float HeadBobRunAmplitude = 2.6f;

    /** Step length walking / sprinting / crouched (cm). Sets the cadence. */
    UPROPERTY(EditAnywhere, Category = "Camera|Headbob")
    float WalkStepLength = 110.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Headbob")
    float RunStepLength = 165.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Headbob")
    float CrouchStepLength = 70.f;

    float HeadBobTimer = 0.f;
    float BobAmount = 0.f;

    // FOV. A bodycam lens doesn't zoom when you sprint, so these are small.
    UPROPERTY(EditAnywhere, Category = "Camera|FOV")
    float BaseFOV = 100.f;

    UPROPERTY(EditAnywhere, Category = "Camera|FOV")
    float SprintFOVAdd = 3.f;

    UPROPERTY(EditAnywhere, Category = "Camera|FOV")
    float SlideFOVAdd = 4.f;

    UPROPERTY(EditAnywhere, Category = "Camera|FOV")
    float FOVInterpSpeed = 6.f;

    // Slide pitch tilt
    float CurrentSlidePitch = 0.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Slide")
    float SlideTiltPitch = -10.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Slide")
    float SlideTiltInterpSpeed = 8.f;

    // Camera shake
    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    bool bEnableCameraShake = true;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float RunShakeAmplitudeDeg = 0.6f;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float RunShakeFrequency = 12.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float LandingShakeAmountDeg = 6.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float ShakeDecaySpeed = 10.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Shake")
    float PosJitterAmplitude = 0.6f;

    float ShakeNoiseTime      = 0.f;
    float LandingShakeImpulse = 0.f;

    // Lean
    bool  bLeanLeftHeld  = false;
    bool  bLeanRightHeld = false;
    float LeanTarget = 0.f;
    float LeanAmount = 0.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Lean")
    float LeanOffset = 22.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Lean")
    float LeanRoll = 8.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Lean")
    float LeanYaw = 2.5f;

    UPROPERTY(EditAnywhere, Category = "Camera|Lean")
    float LeanInterpSpeed = 10.f;

    /** Head radius for the lean collision check. */
    UPROPERTY(EditAnywhere, Category = "Camera|Lean")
    float LeanProbeRadius = 12.f;

    // Head tilt (Q/E)
    bool  bTiltLeftHeld  = false;
    bool  bTiltRightHeld = false;
    float TiltTarget  = 0.f;
    float TiltCurrent = 0.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Tilt")
    float TiltMaxAngle = 20.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Tilt")
    float TiltInterpSpeed = 6.f;

    // Weapon
    UPROPERTY(EditAnywhere, Category = "Weapon")
    TSubclassOf<AGun> GunClass;

    AGun* CurrentGun = nullptr;

    UPROPERTY(EditAnywhere, Category = "Weapon|ADS")
    float AimFOVAdd = 14.f;

    /** How fast the gun comes up to the eye. */
    UPROPERTY(EditAnywhere, Category = "Weapon|ADS")
    float AimInSpeed = 9.f;

    bool bIsAiming = false;
    float AimAlpha = 0.f;

    FRotator RecoilCurrent = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
    float RecoilRecoverSpeed = 8.f;

    /** Fraction of each shot's muzzle climb that stays (you have to pull back down). */
    UPROPERTY(EditAnywhere, Category = "Weapon|Recoil", meta = (ClampMin = 0, ClampMax = 1))
    float RecoilClimb = 0.25f;

    float FOVAdd = 0.f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
    float FOVRecoverSpeed = 6.f;

    // Bodycam sway
    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayPosScale = 28.f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayMaxOffset = 35.f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayRotScalePitch = 4.5f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayRotScaleYaw = 5.0f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayRotScaleRoll = 3.5f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayFollowSpeed = 4.f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayReturnSpeed = 3.f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayVelocitySmoothing = 10.f;

    UPROPERTY(EditAnywhere, Category = "Weapon|Sway")
    float SwayStrideAmplitude = 3.5f;

    /** Sway x this when fully aimed. */
    UPROPERTY(EditAnywhere, Category = "Weapon|Sway", meta = (ClampMin = 0, ClampMax = 1))
    float AimSwayScale = 0.35f;

    FVector  SwayVelocity   = FVector::ZeroVector;
    FVector  SwayTargetLoc  = FVector::ZeroVector;
    FVector  SwayCurrentLoc = FVector::ZeroVector;
    FRotator SwayTargetRot  = FRotator::ZeroRotator;
    FRotator SwayCurrentRot = FRotator::ZeroRotator;

    float IdleSwayTime = 0.f;

    void AddRecoil(float PitchDegrees, float YawDegrees);
    void AddFOVKick(float Amount);
};
