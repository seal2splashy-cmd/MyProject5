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
    }
}

void AGauntAnomaly::PerformAttack()
{
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
