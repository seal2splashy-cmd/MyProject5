// AGauntAnomaly.cpp

#include "AGauntAnomaly.h"
#include "MyFPSPawn.h"
#include "BodycamLensComponent.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "RealismUtils.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
    const TCHAR* GauntFolder = TEXT("/Game/Generated/Anomaly/Gaunt");

    template <typename T>
    TSoftObjectPtr<T> GauntAsset(const TCHAR* SubPath, const TCHAR* Name)
    {
        // e.g. /Game/Generated/Anomaly/Gaunt/Anims/A_Gaunt_Walk.A_Gaunt_Walk
        return TSoftObjectPtr<T>(FSoftObjectPath(
            FString::Printf(TEXT("%s%s/%s.%s"), GauntFolder, SubPath, Name, Name)));
    }

    /** Load a soft reference only if its package exists, so missing optional assets stay quiet. */
    template <typename T>
    T* LoadIfPresent(const TSoftObjectPtr<T>& Ptr)
    {
        if (Ptr.IsNull())
        {
            return nullptr;
        }
        if (T* Loaded = Ptr.Get())
        {
            return Loaded;
        }
        return FPackageName::DoesPackageExist(Ptr.GetLongPackageName()) ? Ptr.LoadSynchronous() : nullptr;
    }
}

AGauntAnomaly::AGauntAnomaly()
{
    // 1.90 m tall, thin.
    GetCapsuleComponent()->InitCapsuleSize(34.f, 95.f);

    StandIn = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StandIn"));
    StandIn->SetupAttachment(GetCapsuleComponent());
    StandIn->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // the capsule takes the hits
    StandIn->SetRelativeRotation(FRotator(0.f, MeshYaw, 0.f));

    // A lanky stalker: a bit slower than the base enemy, but long arms.
    MoveSpeed = 400.f;
    AttackRange = 175.f;
    EyeHeight = 80.f;

    // Something that lives in the dark: sees better in it, hears better, heavy slow steps.
    DarkSightRange = 1100.f;
    HearingScale = 1.3f;
    FootstepPitch = 0.68f;
    FootstepVolume = 0.9f;

    StandInMesh = GauntAsset<UStaticMesh>(TEXT(""), TEXT("SM_AH_Gaunt"));
    RiggedMesh  = GauntAsset<USkeletalMesh>(TEXT(""), TEXT("SK_AH_Gaunt"));
    IdleAnim    = GauntAsset<UAnimationAsset>(TEXT("/Anims"), TEXT("A_Gaunt_Idle"));
    WalkAnim    = GauntAsset<UAnimationAsset>(TEXT("/Anims"), TEXT("A_Gaunt_Walk"));
    RunAnim     = GauntAsset<UAnimationAsset>(TEXT("/Anims"), TEXT("A_Gaunt_Run"));
    AttackAnim  = GauntAsset<UAnimationAsset>(TEXT("/Anims"), TEXT("A_Gaunt_Attack"));
    DyingAnim   = GauntAsset<UAnimationAsset>(TEXT("/Anims"), TEXT("A_Gaunt_Death"));
}

void AGauntAnomaly::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyVisuals();   // so it shows up properly when placed in the editor
}

void AGauntAnomaly::ApplyVisuals()
{
    USkeletalMeshComponent* Skel = GetMesh();
    const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

    USkeletalMesh* Rig = LoadIfPresent(RiggedMesh);
    if (!Rig && Skel)
    {
        Rig = Skel->GetSkeletalMeshAsset();   // a Blueprint child may have assigned its own
    }

    if (Rig && Skel)
    {
        Skel->SetSkeletalMeshAsset(Rig);
        // Mixamo/Blender characters have their origin at the feet.
        Skel->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -HalfHeight), FRotator(0.f, MeshYaw, 0.f));
        Skel->SetVisibility(true);
        StandIn->SetStaticMesh(nullptr);
        StandIn->SetVisibility(false);
        bRigged = true;
        return;
    }

    bRigged = false;
    if (UStaticMesh* StandInAsset = LoadIfPresent(StandInMesh))
    {
        // The Meshy model's origin is the middle of the body = the capsule centre.
        StandIn->SetStaticMesh(StandInAsset);
        StandIn->SetVisibility(true);
        StandIn->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator(0.f, MeshYaw, 0.f));
    }
}

void AGauntAnomaly::BeginPlay()
{
    ApplyVisuals();   // before Super, so AEnemy sees a body and doesn't show the debug capsule

    if (bRigged)
    {
        LoadedIdle   = LoadIfPresent(IdleAnim);
        LoadedWalk   = LoadIfPresent(WalkAnim);
        LoadedRun    = LoadIfPresent(RunAnim);
        LoadedAttack = LoadIfPresent(AttackAnim);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    }
    if (!bRigged || !LoadIfPresent(DyingAnim))
    {
        DyingAnim.Reset();   // nothing to play; AEnemy goes straight to ragdoll
    }

    Super::BeginPlay();

    NextTwitchTime = static_cast<float>(GetWorld()->GetTimeSeconds() + FMath::FRandRange(TwitchInterval.X, TwitchInterval.Y));
    NextClickTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + FMath::FRandRange(ClickInterval.X, ClickInterval.Y);

    // Its breathing, always on, carried with it (walls muffle it through the attenuation)
    if (USoundBase* Breath = AHRealism::GeneratedSound(TEXT("SW_Gaunt_Breath_Loop")))
    {
        BreathAudio = UGameplayStatics::SpawnSoundAttached(Breath, GetRootComponent(), NAME_None,
            FVector(0.f, 0.f, 60.f), EAttachLocation::KeepRelativeOffset, true, 0.5f, 1.f,
            FMath::FRandRange(0.f, 4.f), AHRealism::Attenuation(BreathRange), nullptr, false);
    }
}

void AGauntAnomaly::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);   // the AI; does nothing once dead

    if (bRigged)
    {
        if (!bDead)
        {
            UpdateRiggedAnimation();
        }
    }
    else
    {
        UpdateStandIn(DeltaTime);   // also runs the death topple
    }

    if (!bDead)
    {
        UpdateInterference();
        UpdateVoice(DeltaTime);
        UpdateGrab();
    }
}

// ------------------------------------------------------------------ hunting

void AGauntAnomaly::HandleState_Chase(float DeltaTime)
{
    AMyFPSPawn* Player = Cast<AMyFPSPawn>(PlayerPawn);
    if (bGrabbing)
    {
        MoveTowards(GetActorLocation(), DeltaTime, 1000000.f, 0.f);    // hold on
        FacePlayer(DeltaTime);
        return;
    }
    if (!Player || Player->IsDead())
    {
        Super::HandleState_Chase(DeltaTime);
        return;
    }

    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    const FVector Head = GetPawnViewLocation();
    const float Dist = static_cast<float>(FVector::Dist(GetActorLocation(), Player->GetActorLocation()));

    // The light: up close the beam makes it shriek and back off into the dark
    if (Now < FlinchUntil)
    {
        FVector Away = GetActorLocation() - Player->GetActorLocation();
        Away.Z = 0.f;
        MoveTowards(GetActorLocation() + Away.GetSafeNormal() * 500.f, DeltaTime, 60.f, 1.15f);
        return;
    }
    if (Now >= FlinchCooldownUntil && Dist < TorchFlinchDistance && Player->IsTorchShiningAt(Head))
    {
        FlinchUntil = Now + FlinchDuration;
        FlinchCooldownUntil = FlinchUntil + FlinchCooldown;
        Vocalise(TEXT("SW_Gaunt_Screech"), 1, 1.f, 3500.f);
        TwitchRot = FRotator(16.f, FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(-12.f, 12.f));   // recoils
        Lunge = 0.f;
        if (UBodycamLensComponent* Lens = Player->GetBodycamLens())
        {
            Lens->ForceGlitch(0.5f, false);
        }
        return;
    }

    // Stalking: at a distance it only moves while you aren't looking at it
    if (Dist > StalkDistance && CanDetectPlayer(true))
    {
        UpdatePlayerObservation(DeltaTime);
        const FVector ToMe = (Head - Player->GetPawnViewLocation()).GetSafeNormal();
        bStalkFrozen = FVector::DotProduct(Player->GetViewRotation().Vector(), ToMe) > LookedAtCosine;
        if (bStalkFrozen)
        {
            MoveTowards(GetActorLocation(), DeltaTime, 1000000.f, 0.f);
            FacePlayer(DeltaTime);
        }
        else
        {
            MoveTowards(Player->GetActorLocation(), DeltaTime, 60.f, StalkSpeedScale);
        }
        return;
    }
    bStalkFrozen = false;
    Super::HandleState_Chase(DeltaTime);   // close: the learned chase (rush / flank / circle...) and attack
}

void AGauntAnomaly::UpdateGrab()
{
    if (!bGrabbing)
    {
        return;
    }
    Lunge = FMath::Max(Lunge, 0.8f);                 // leaning right into your face
    AMyFPSPawn* Player = Cast<AMyFPSPawn>(PlayerPawn);
    if (!Player || Player->IsDead())
    {
        bGrabbing = false;
        return;
    }
    if (GetWorld()->GetTimeSeconds() >= GrabEndTime)
    {
        bGrabbing = false;
        ASmartEnemy::PerformAttack();                // the blow (and the learning reward) lands now
    }
}

// ------------------------------------------------------------------ voice

void AGauntAnomaly::Vocalise(const TCHAR* Prefix, int32 Variants, float Volume, float Range)
{
    AHRealism::PlayAt(this, AHRealism::RandomVariant(Prefix, Variants), GetPawnViewLocation(), Volume,
        FMath::FRandRange(0.92f, 1.06f), Range);
}

void AGauntAnomaly::UpdateVoice(float DeltaTime)
{
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    const bool bHunting = (State == EAIState::Chase || State == EAIState::Attack);

    // It's seen you
    if (bHunting && (PrevAIState == EAIState::Patrol || PrevAIState == EAIState::Search))
    {
        Vocalise(TEXT("SW_Gaunt_Growl"), 1, 1.f, 3000.f);
    }
    PrevAIState = State;

    if (BreathAudio)
    {
        const float Want = bGrabbing ? 1.3f : (bHunting ? 1.f : (State == EAIState::Search ? 0.75f : 0.45f));
        BreathAudio->SetVolumeMultiplier(FMath::FInterpTo(BreathAudio->VolumeMultiplier, Want, DeltaTime, 2.f));
        BreathAudio->SetPitchMultiplier(bHunting ? 1.12f : 1.f);   // breathing faster
    }

    if (Now >= NextClickTime)
    {
        Vocalise(TEXT("SW_Gaunt_Click"), 3, bStalkFrozen ? 1.f : 0.75f, 2200.f);
        const float Scale = bStalkFrozen ? 0.4f : (bHunting ? 0.7f : 1.f);   // clicks at you while you stare
        NextClickTime = Now + FMath::FRandRange(ClickInterval.X, ClickInterval.Y) * Scale;
    }
}

void AGauntAnomaly::PerformAttack()
{
    if (bGrabbing)
    {
        return;
    }
    AMyFPSPawn* Player = Cast<AMyFPSPawn>(PlayerPawn);
    if (GrabDuration > 0.f && Player && !Player->IsDead() && Player->GetHealth() <= AttackDamage + 0.5f)
    {
        // A killing blow is a grab first
        bGrabbing = true;
        GrabEndTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + GrabDuration;
        Player->Grab(this, GrabDuration + 0.3f);
        Vocalise(TEXT("SW_Gaunt_Scream"), 1, 1.f, 5000.f);
        Lunge = 1.f;
        if (bRigged && LoadedAttack)
        {
            AttackAnimUntil = static_cast<float>(GetWorld()->GetTimeSeconds()) + LoadedAttack->GetPlayLength();
            CurrentAnim = nullptr;
        }
        return;
    }

    Super::PerformAttack();

    Lunge = 1.f;
    if (bRigged && LoadedAttack)
    {
        AttackAnimUntil = static_cast<float>(GetWorld()->GetTimeSeconds()) + LoadedAttack->GetPlayLength();
        CurrentAnim = nullptr;   // restart the clip even if it was already playing
    }
}

void AGauntAnomaly::OnDeath()
{
    Super::OnDeath();   // Q-table bookkeeping, stop moving, death clip + ragdoll when rigged

    bGrabbing = false;
    if (BreathAudio)
    {
        BreathAudio->FadeOut(1.5f, 0.f);
    }
    Vocalise(TEXT("SW_Gaunt_Screech"), 1, 1.f, 4000.f);

    if (!bRigged)
    {
        // Topple away from whoever killed it.
        FallAlpha = 0.f;
        FallSign = 1.f;
        if (PlayerPawn)
        {
            const FVector Away = GetActorLocation() - PlayerPawn->GetActorLocation();
            FallSign = FVector::DotProduct(Away, GetActorForwardVector()) >= 0.f ? 1.f : -1.f;
        }
        FallSide = FMath::FRandRange(-0.35f, 0.35f);
    }
}

// ------------------------------------------------------------------ rigged

void AGauntAnomaly::UpdateRiggedAnimation()
{
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    const float Speed = static_cast<float>(GetVelocity().Size2D());

    UAnimationAsset* Want = LoadedIdle;
    bool bLoop = true;
    if (LoadedAttack && Now < AttackAnimUntil)
    {
        Want = LoadedAttack;
        bLoop = false;
    }
    else if (Speed > RunSpeedThreshold && (LoadedRun || LoadedWalk))
    {
        Want = LoadedRun ? LoadedRun.Get() : LoadedWalk.Get();
    }
    else if (Speed > 15.f && LoadedWalk)
    {
        Want = LoadedWalk;
    }

    if (Want && Want != CurrentAnim)
    {
        GetMesh()->PlayAnimation(Want, bLoop);
        CurrentAnim = Want;
    }
}

// ------------------------------------------------------------------ stand-in

void AGauntAnomaly::UpdateStandIn(float DeltaTime)
{
    if (!StandIn || !StandIn->GetStaticMesh())
    {
        return;
    }

    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const FVector Feet(0.f, 0.f, -HalfHeight);

    FQuat Offset = FQuat::Identity;   // in the actor's frame: pitch < 0 leans forwards
    FVector Lift = FVector::ZeroVector;

    if (FallAlpha >= 0.f)
    {
        // Dead: tip over from the feet, accelerating like a falling plank.
        FallAlpha = FMath::Min(1.f, FallAlpha + DeltaTime / 0.8f);
        const float T = FallAlpha * FallAlpha;
        const float Angle = 84.f * T;
        Offset = FRotator(-FallSign * Angle, 0.f, FallSide * Angle).Quaternion();
        Lift = FVector(0.f, 0.f, 12.f * T);   // it's ~20 cm thick lying down
    }
    else
    {
        const float Speed = static_cast<float>(GetVelocity().Size2D());
        SmoothedSpeed = FMath::FInterpTo(SmoothedSpeed, Speed, DeltaTime, 6.f);
        const float Move01 = FMath::Clamp(SmoothedSpeed / FMath::Max(MoveSpeed, 1.f), 0.f, 1.f);

        StridePhase += DeltaTime * SmoothedSpeed / FMath::Max(StrideLength, 1.f) * UE_PI;
        const float Bob = FMath::Abs(FMath::Sin(StridePhase)) * BobHeight * Move01;
        const float Sway = FMath::Sin(StridePhase) * SwayDegrees * Move01;
        const float Breath = FMath::Sin(Now * 1.3f) * 1.5f * (1.f - Move01);

        // Nervous twitches, more often while it's hunting you.
        if (Now >= NextTwitchTime)
        {
            TwitchRot = FRotator(FMath::FRandRange(-0.5f, 0.5f) * TwitchDegrees,
                                 FMath::FRandRange(-1.f, 1.f) * TwitchDegrees,
                                 FMath::FRandRange(-0.7f, 0.7f) * TwitchDegrees);
            const bool bHunting = (State == EAIState::Chase || State == EAIState::Attack);
            NextTwitchTime = Now + static_cast<float>(FMath::FRandRange(TwitchInterval.X, TwitchInterval.Y)) * (bHunting ? 0.5f : 1.f);
        }
        TwitchRot = FMath::RInterpTo(TwitchRot, FRotator::ZeroRotator, DeltaTime, 7.f);
        Lunge = FMath::FInterpTo(Lunge, 0.f, DeltaTime, 5.f);

        const float Lean = MaxLean * Move01 + 22.f * Lunge + Breath;
        Offset = (FRotator(-Lean, 0.f, Sway) + TwitchRot).Quaternion();
        Lift = FVector(0.f, 0.f, Bob);
    }

    // Pivot about the feet so leaning/falling doesn't slide them along the ground.
    const FQuat Rot = Offset * FRotator(0.f, MeshYaw, 0.f).Quaternion();
    const FVector Loc = Feet - Offset.RotateVector(Feet) + Lift;
    StandIn->SetRelativeLocationAndRotation(Loc, Rot);
}

// ------------------------------------------------------------------ bodycam

void AGauntAnomaly::UpdateInterference()
{
    AMyFPSPawn* Player = Cast<AMyFPSPawn>(PlayerPawn);
    UBodycamLensComponent* Lens = Player ? Player->GetBodycamLens() : nullptr;
    if (!Lens || InterferenceStartDistance <= InterferenceFullDistance)
    {
        return;
    }

    const float Dist = static_cast<float>(FVector::Dist(GetActorLocation(), Player->GetActorLocation()));
    if (Dist >= InterferenceStartDistance)
    {
        return;
    }

    float Amount = 1.f - FMath::Clamp((Dist - InterferenceFullDistance) /
        (InterferenceStartDistance - InterferenceFullDistance), 0.f, 1.f);
    Amount *= Amount;   // faint at the edge, ramps up as it closes in
    if (State == EAIState::Chase || State == EAIState::Attack)
    {
        Amount += HuntingInterferenceBoost;
    }
    Lens->ReportInterference(FMath::Clamp(Amount, 0.f, 1.f));
}
