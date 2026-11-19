// AEnemy.cpp
// [Rebuilt Oct 2026 from coursework excerpts; senses, hearing, Search and NavMesh added Nov 2026]

#include "AEnemy.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "MyFPSPawn.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RealismUtils.h"
#include "TimerManager.h"

// Console: `ah.DebugNoise 1` draws every noise (yellow sphere = how far it carries),
// `ah.DebugSenses 1` draws each enemy's vision cone, sight range and awareness.
static TAutoConsoleVariable<int32> CVarDebugNoise(TEXT("ah.DebugNoise"), 0,
    TEXT("1 = draw every noise enemies can hear (sphere = range)."), ECVF_Cheat);
static TAutoConsoleVariable<int32> CVarDebugSenses(TEXT("ah.DebugSenses"), 0,
    TEXT("1 = draw enemy vision cones, sight range and awareness."), ECVF_Cheat);

AEnemy::AEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    bUseControllerRotationYaw = false;
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0.f, 540.f, 0.f);
        Move->MaxWalkSpeed = MoveSpeed;
    }

    // Make sure bullets and melee sweeps (Pawn / Visibility) register on the body
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionProfileName(TEXT("CharacterMesh"));
        Skel->SetGenerateOverlapEvents(false);
    }

    DyingAnim = TSoftObjectPtr<UAnimationAsset>(FSoftObjectPath(TEXT("/Game/Dying.Dying")));
}

void AEnemy::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    SpawnLocation = GetActorLocation();
    CurrentPatrolTarget = PickNewPatrolPoint();
    PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

    // No body at all (no skeletal mesh, and no static mesh a subclass might use instead)?
    // Show the capsule so you can still see and test it.
    bool bHasStaticBody = false;
    TInlineComponentArray<UStaticMeshComponent*> StaticMeshes(this);
    for (const UStaticMeshComponent* SM : StaticMeshes)
    {
        bHasStaticBody |= (SM->GetStaticMesh() != nullptr && SM->IsVisible());
    }
    if (!bHasStaticBody && (!GetMesh() || !GetMesh()->GetSkeletalMeshAsset()))
    {
        UE_LOG(LogTemp, Warning, TEXT("%s has no skeletal mesh - make a Blueprint child and assign one."), *GetName());
        if (UCapsuleComponent* Cap = GetCapsuleComponent())
        {
            Cap->SetHiddenInGame(false);
        }
    }
}

void AEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bDead) return;

    if (!PlayerPawn)
    {
        PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    }

    switch (State)
    {
    case EAIState::Patrol: HandleState_Patrol(DeltaTime); break;
    case EAIState::Chase:  HandleState_Chase(DeltaTime);  break;
    case EAIState::Attack: HandleState_Attack(DeltaTime); break;
    case EAIState::Search: HandleState_Search(DeltaTime); break;
    case EAIState::Dead:   break;
    }

    if (State == EAIState::Chase || State == EAIState::Attack)
    {
        Awareness = 1.f;   // it knows
    }

    UpdateFootsteps(DeltaTime);

    if (CVarDebugSenses.GetValueOnGameThread() != 0)
    {
        DrawDebugSenses();
    }
}

void AEnemy::SwitchState(EAIState NewState)
{
    if (State == NewState) return;
    State = NewState;

    ClearPath();   // whatever we were walking to, the new state picks its own target

    switch (State)
    {
    case EAIState::Patrol:
        CurrentPatrolTarget = PickNewPatrolPoint();
        PatrolTargetTime = GetWorld()->GetTimeSeconds();
        break;
    case EAIState::Chase:
    case EAIState::Attack:
    case EAIState::Search:
    case EAIState::Dead:
        break;
    }
}

// ------------------------------------------------------------------ states

void AEnemy::HandleState_Patrol(float DeltaTime)
{
    if (UpdateAwareness(DeltaTime))
    {
        return;   // spotted (Chase) or suspicious (Search)
    }

    MoveTowards(CurrentPatrolTarget, DeltaTime, PatrolAcceptRadius, 0.5f);

    // reached it, or been trying for ages (unreachable / stuck on something): pick another
    const float Now = GetWorld()->GetTimeSeconds();
    if (FVector::Dist2D(GetActorLocation(), CurrentPatrolTarget) <= PatrolAcceptRadius ||
        Now - PatrolTargetTime > PatrolGiveUpTime)
    {
        CurrentPatrolTarget = PickNewPatrolPoint();
        PatrolTargetTime = Now;
        ClearPath();
    }
}

void AEnemy::HandleState_Chase(float DeltaTime)
{
    if (!PlayerPawn) { SwitchState(EAIState::Patrol); return; }

    const bool bSeen = CanDetectPlayer(true);
    if (bSeen)
    {
        UpdatePlayerObservation(DeltaTime);
    }

    const float Now = GetWorld()->GetTimeSeconds();
    const bool bReachedLastSeen = !bSeen && FVector::Dist2D(GetActorLocation(), LastSeenPlayerLocation) < 150.f;
    if (!bSeen && ((Now - LastSeenTime) > GiveUpTime || bReachedLastSeen))
    {
        // Lost them: look around where they were last seen instead of wandering off.
        BeginSearch(LastSeenPlayerLocation, true);
        return;
    }

    const FVector Target = bSeen ? PredictPlayerLocation() : LastSeenPlayerLocation;
    MoveTowards(Target, DeltaTime, AttackRange * 0.8f, 1.f);

    // close AND in view - right behind a wall doesn't count, it has to path round first
    if (bSeen && FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation()) <= AttackRange)
    {
        SwitchState(EAIState::Attack);
    }
}

void AEnemy::HandleState_Attack(float DeltaTime)
{
    if (!PlayerPawn)
    {
        SwitchState(EAIState::Patrol);
        return;
    }
    if (!CanDetectPlayer(true))
    {
        // Ducked behind something: go and look where they were (Chase -> Search after GiveUpTime)
        SwitchState(EAIState::Chase);
        return;
    }

    if (FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation()) > AttackRange * 1.2f)
    {
        SwitchState(EAIState::Chase);
        return;
    }

    FacePlayer(DeltaTime);

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastAttackTime >= AttackCooldown)
    {
        LastAttackTime = Now;
        PerformAttack();
    }
}

void AEnemy::BeginSearch(const FVector& Center, bool bUrgent)
{
    const float Now = GetWorld()->GetTimeSeconds();
    SearchCenter = Center;
    SearchPoint = Center;
    bSearchUrgent = bUrgent;
    bSearchReachedCenter = false;
    SearchEndTime = Now + SearchDuration;
    SearchLegStartTime = Now;
    SearchLookUntil = 0.f;
    Awareness = FMath::Max(Awareness, SuspicionLevel);

    if (State == EAIState::Search)
    {
        ClearPath();   // already searching: just head for the new spot
    }
    else
    {
        SwitchState(EAIState::Search);
    }
}

void AEnemy::HandleState_Search(float DeltaTime)
{
    if (UpdateAwareness(DeltaTime))
    {
        return;   // found them
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now > SearchEndTime)
    {
        Awareness = FMath::Min(Awareness, SuspicionLevel * 0.5f);
        SwitchState(EAIState::Patrol);
        return;
    }

    // Standing still, looking about
    if (Now < SearchLookUntil)
    {
        MoveTowards(GetActorLocation(), DeltaTime, 1000000.f, 0.f);   // brake
        const FRotator Want(0.f, SearchLookYaw, 0.f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaTime, 2.5f));
        if (FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, SearchLookYaw)) < 6.f)
        {
            SearchLookYaw = GetActorRotation().Yaw + FMath::FRandRange(70.f, 130.f) * (FMath::RandBool() ? 1.f : -1.f);
        }
        return;
    }

    const FVector Goal = bSearchReachedCenter ? SearchPoint : SearchCenter;
    const float Speed = (bSearchUrgent && !bSearchReachedCenter) ? 0.95f : SearchSpeedScale;
    MoveTowards(Goal, DeltaTime, 80.f, Speed);

    if (FVector::Dist2D(GetActorLocation(), Goal) <= 130.f || Now - SearchLegStartTime > 9.f)
    {
        // Got there (or can't): stop and look around, then try somewhere else nearby
        bSearchReachedCenter = true;
        SearchLookUntil = Now + FMath::FRandRange(1.5f, 3.f);
        SearchLookYaw = GetActorRotation().Yaw + FMath::FRandRange(-120.f, 120.f);
        SearchPoint = RandomReachablePoint(SearchCenter, SearchRadius);
        SearchLegStartTime = SearchLookUntil;   // the next leg's clock starts once it's done looking
        ClearPath();
    }
}

// ------------------------------------------------------------------ senses

bool AEnemy::SeesPlayer(float* OutCloseness) const
{
    if (!PlayerPawn) return false;

    const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const FVector Head = PlayerPawn->GetPawnViewLocation();
    const FVector ToPlayer = Head - Eye;
    const float Dist = ToPlayer.Size();

    // How visible they are: light they're standing in, torch, crouching (see AMyFPSPawn::GetVisibility01)
    float Visibility = 1.f;
    bool bTorchInMyFace = false;
    if (const AMyFPSPawn* Player = Cast<AMyFPSPawn>(PlayerPawn))
    {
        if (bUseLighting)
        {
            Visibility = Player->GetVisibility01();
        }
        bTorchInMyFace = Player->IsTorchShiningAt(Eye);
    }

    float Range = FMath::Lerp(DarkSightRange, DetectionRadius, Visibility);
    if (bTorchInMyFace)
    {
        Range = FMath::Max(Range, DetectionRadius * 1.2f);   // a torch in the dark is seen a long way off
    }
    if (Dist > Range)
    {
        return false;
    }

    // Field of view - unless they're right next to it or shining a torch at it
    if (Dist > CloseSenseRadius && !bTorchInMyFace)
    {
        const bool bAlert = (State == EAIState::Chase || State == EAIState::Attack);
        const float HalfAngle = bAlert ? AlertVisionHalfAngle : VisionHalfAngle;
        const float CosAngle = FVector::DotProduct(GetActorForwardVector(), ToPlayer / FMath::Max(Dist, 1.f));
        if (CosAngle < FMath::Cos(FMath::DegreesToRadians(HalfAngle)))
        {
            return false;
        }
    }

    if (bUseLineOfSight && !HasLineOfSightToPlayer())
    {
        return false;
    }

    if (OutCloseness)
    {
        *OutCloseness = FMath::Clamp(1.f - Dist / FMath::Max(Range, 1.f), 0.f, 1.f);
    }
    return true;
}

bool AEnemy::CanDetectPlayer(bool bUpdateMemory)
{
    if (!PlayerPawn) return false;

    const float Now = GetWorld()->GetTimeSeconds();
    if (SeesPlayer())
    {
        if (bUpdateMemory)
        {
            LastSeenPlayerLocation = PlayerPawn->GetActorLocation();
            LastSeenTime = Now;
        }
        return true;
    }

    // Without line of sight rules (old mode): awareness persists a little beyond sight range
    if (!bUseLineOfSight)
    {
        const float Dist = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
        if (Dist <= LoseSightRadius && (Now - LastSeenTime) <= GiveUpTime)
            return true;
    }
    return false;
}

bool AEnemy::UpdateAwareness(float DeltaTime)
{
    float Closeness = 0.f;
    if (SeesPlayer(&Closeness))
    {
        // Takes a moment to register someone at the edge of sight; almost instant up close.
        const float Rate = AwarenessGain * (0.35f + 1.65f * Closeness) * (State == EAIState::Search ? 1.6f : 1.f);
        Awareness = FMath::Min(1.f, Awareness + Rate * DeltaTime);

        if (Awareness >= 1.f)
        {
            LastSeenPlayerLocation = PlayerPawn->GetActorLocation();
            LastSeenTime = GetWorld()->GetTimeSeconds();
            bHasObservation = false;
            SwitchState(EAIState::Chase);
            return true;
        }
        if (State == EAIState::Patrol && Awareness >= SuspicionLevel)
        {
            BeginSearch(PlayerPawn->GetActorLocation(), false);   // "what was that?"
            return true;
        }
        if (State == EAIState::Search)
        {
            SearchCenter = PlayerPawn->GetActorLocation();   // keep looking where the shape was
        }
    }
    else
    {
        Awareness = FMath::Max(0.f, Awareness - AwarenessDecay * DeltaTime);
    }
    return false;
}

bool AEnemy::HasLineOfSightToPlayer() const
{
    if (!PlayerPawn) return false;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(AEnemyLOS), true, this);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(PlayerPawn);

    // The player's gun, melee weapon and anything else they're carrying doesn't hide them.
    TArray<AActor*> Carried;
    PlayerPawn->GetAttachedActors(Carried, true, true);
    Params.AddIgnoredActors(Carried);

    // Two rays, to the player's head (the camera - so leaning round a corner shows your head)
    // and chest: peeking over a crate still gets you seen.
    const FVector EyeLoc = GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const FVector Targets[2] = { PlayerPawn->GetPawnViewLocation(), PlayerPawn->GetActorLocation() };
    for (const FVector& To : Targets)
    {
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, EyeLoc, To, ECC_Visibility, Params))
        {
            return true;
        }
        const AActor* Blocker = Hit.GetActor();
        if (Blocker && (Blocker->GetOwner() == PlayerPawn || Blocker->GetAttachParentActor() == PlayerPawn))
        {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------ hearing

void AEnemy::ReportNoise(const UObject* WorldContext, const FVector& Location, float Range, AActor* NoiseMaker)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || Range <= 1.f)
    {
        return;
    }

    if (CVarDebugNoise.GetValueOnGameThread() != 0)
    {
        DrawDebugSphere(World, Location, Range, 20, FColor::Yellow, false, 1.2f, 0, 1.f);
    }

    for (TActorIterator<AEnemy> It(World); It; ++It)
    {
        AEnemy* Enemy = *It;
        if (Enemy && Enemy != NoiseMaker)
        {
            Enemy->HearNoise(Location, Range, NoiseMaker);
        }
    }
}

void AEnemy::HearNoise(const FVector& Location, float Range, AActor* NoiseMaker)
{
    if (bDead || HearingScale <= 0.f) return;

    const FVector Ear = GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const float Dist = FVector::Dist(Ear, Location);
    float Effective = Range * HearingScale;
    if (Dist > Effective) return;

    // Through a wall it's muffled and doesn't carry as far
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(AEnemyHearing), false, this);
    if (NoiseMaker)
    {
        Params.AddIgnoredActor(NoiseMaker);
    }
    if (PlayerPawn)
    {
        Params.AddIgnoredActor(PlayerPawn);
    }
    if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 10.f), Ear, ECC_Visibility, Params))
    {
        Effective *= WallMuffle;
        if (Dist > Effective) return;
    }

    const float Loudness = 1.f - Dist / FMath::Max(Effective, 1.f);   // 1 = right next to it
    const float Now = GetWorld()->GetTimeSeconds();

    // It can't pinpoint a sound exactly
    const FVector2D Err = FMath::RandPointInCircle(NoiseLocationError * Dist);
    const FVector Guess = Location + FVector(Err.X, Err.Y, 0.f);

    switch (State)
    {
    case EAIState::Chase:
    case EAIState::Attack:
        // Already hunting: noises the player makes tell it where they've gone
        if (NoiseMaker == PlayerPawn && !SeesPlayer())
        {
            LastSeenPlayerLocation = Guess;
            LastSeenTime = Now;
        }
        break;

    case EAIState::Patrol:
    case EAIState::Search:
        // Don't re-target on every footstep: only a louder noise, or a new one after a pause
        if (State == EAIState::Search && Loudness <= LastHeardLoudness && Now - LastHeardTime < 1.5f)
        {
            break;
        }
        LastHeardTime = Now;
        LastHeardLoudness = Loudness;
        BeginSearch(Guess, Loudness > 0.35f);
        break;

    case EAIState::Dead:
        break;
    }
}

// ------------------------------------------------------------------ observation / prediction

void AEnemy::UpdatePlayerObservation(float DeltaTime)
{
    if (!PlayerPawn) return;

    const FVector CurrPos = PlayerPawn->GetActorLocation();

    if (bHasObservation)
    {
        // Velocity estimate (player is an APawn with manual movement, so measure it)
        const FVector MeasuredVel = (CurrPos - LastObservedPlayerPos)
            / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
        PlayerVelocityEstimate = FMath::VInterpTo(
            PlayerVelocityEstimate, MeasuredVel, DeltaTime, 6.f);

        // Adaptive bias: drift towards our own prediction error
        const FVector Error = CurrPos - LastPrediction;
        PredictionBias = FMath::VInterpTo(
            PredictionBias, Error, DeltaTime, LearningRate);
        PredictionBias = PredictionBias.GetClampedToMaxSize(400.f);
    }

    LastObservedPlayerPos = CurrPos;
    bHasObservation = true;
}

FVector AEnemy::PredictPlayerLocation()
{
    if (!PlayerPawn) return GetActorLocation();

    const FVector CurrPos = PlayerPawn->GetActorLocation();
    FVector Predicted = CurrPos + PlayerVelocityEstimate * PredictionTime + PredictionBias;
    Predicted.Z = CurrPos.Z;

    // what we predicted for "now + PredictionTime" - compared against later
    LastPrediction = CurrPos + PlayerVelocityEstimate * PredictionTime;
    return Predicted;
}

// ------------------------------------------------------------------ movement

void AEnemy::MoveTowards(const FVector& Target, float DeltaTime, float StopDist, float SpeedScale)
{
    const FVector MyLoc = GetActorLocation();
    UCharacterMovementComponent* Move = GetCharacterMovement();

    if (SpeedScale <= 0.f)   // "stand still" (HoldPosition / looking around): just brake
    {
        if (Move)
            Move->Velocity = FMath::VInterpTo(Move->Velocity, FVector::ZeroVector, DeltaTime, 8.f);
        return;
    }

    // Re-plan when the target has moved enough or there's no path yet - but no more often than
    // MinRepathInterval (the chase target moves every tick, and a target with no route mustn't
    // cost a path query every frame) - and every MaxRepathInterval anyway.
    const float Now = GetWorld()->GetTimeSeconds();
    const float SincePlan = Now - PathPlannedTime;
    const bool bTargetMoved = FVector::Dist2D(Target, PathGoal) > RepathDistance;
    if (bUseNavMesh &&
        (SincePlan > MaxRepathInterval || (SincePlan > MinRepathInterval && (!bHasPath || bTargetMoved))))
    {
        PlanPath(Target);
    }

    // Where to steer now, and how far there is still to go
    FVector Steer = Target;
    float Remaining = FVector::Dist2D(MyLoc, Target);
    if (bHasPath)
    {
        // corners we've reached (never skip the last point)
        while (PathIndex < PathPoints.Num() - 1 &&
               FVector::Dist2D(MyLoc, PathPoints[PathIndex]) <= WaypointAcceptRadius)
        {
            ++PathIndex;
        }
        Steer = PathPoints[PathIndex];
        Remaining = RemainingPathLength(MyLoc);

        if (bDrawDebugPath)
        {
            FVector From = MyLoc;
            for (int32 i = PathIndex; i < PathPoints.Num(); ++i)
            {
                DrawDebugLine(GetWorld(), From, PathPoints[i] + FVector(0, 0, 20), FColor::Green, false, -1.f, 0, 3.f);
                From = PathPoints[i] + FVector(0, 0, 20);
            }
        }
    }

    if (Remaining <= StopDist || Remaining <= KINDA_SMALL_NUMBER)
    {
        if (Move)
            Move->Velocity = FMath::VInterpTo(Move->Velocity, FVector::ZeroVector, DeltaTime, 8.f);
        return;
    }

    FVector ToSteer = Steer - MyLoc;
    ToSteer.Z = 0.f;
    if (ToSteer.IsNearlyZero())
    {
        return;
    }
    const FVector Dir = ToSteer.GetSafeNormal();
    const float InputScale = FMath::Clamp((Remaining - StopDist) / 600.f, 0.25f, 1.0f) * SpeedScale;
    AddMovementInput(Dir, InputScale);

    if (Move)
        Move->MaxWalkSpeed = MoveSpeed * SpeedScale;
}

bool AEnemy::PlanPath(const FVector& Target)
{
    const float Now = GetWorld()->GetTimeSeconds();

    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav || !Nav->GetDefaultNavDataInstance())
    {
        if (!bWarnedNoNavMesh)
        {
            bWarnedNoNavMesh = true;
            UE_LOG(LogTemp, Warning, TEXT("%s: no NavMesh in this level - walking in straight lines. "
                "Run Tools/add_navmesh_ue58.py with the level open."), *GetName());
        }
        ClearPath();
        PathGoal = Target;
        PathPlannedTime = Now;   // don't ask again until the next interval
        return false;
    }

    // Flank / retreat / circle / search targets can land inside a wall: snap them onto the NavMesh first.
    FVector Goal = Target;
    FNavLocation OnNav;
    if (Nav->ProjectPointToNavigation(Target, OnNav, FVector(300.f, 300.f, 250.f)))
    {
        Goal = OnNav.Location;
    }

    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), Goal, this);
    if (!Path || !Path->IsValid() || Path->PathPoints.Num() < 2)
    {
        ClearPath();   // no route (e.g. the target is on a roof): fall back to walking straight at it
        PathGoal = Target;
        PathPlannedTime = Now;
        return false;
    }

    PathPoints = Path->PathPoints;   // [0] is where we are now
    PathIndex = 1;
    PathGoal = Target;
    PathPlannedTime = Now;
    bHasPath = true;
    return true;
}

void AEnemy::ClearPath()
{
    PathPoints.Reset();
    PathIndex = 0;
    bHasPath = false;
    PathPlannedTime = -1000.f;
}

float AEnemy::RemainingPathLength(const FVector& MyLoc) const
{
    if (!PathPoints.IsValidIndex(PathIndex)) return 0.f;
    float Length = FVector::Dist2D(MyLoc, PathPoints[PathIndex]);
    for (int32 i = PathIndex + 1; i < PathPoints.Num(); ++i)
    {
        Length += FVector::Dist2D(PathPoints[i - 1], PathPoints[i]);
    }
    return Length;
}

void AEnemy::FacePlayer(float DeltaTime)
{
    if (!PlayerPawn) return;
    FVector To = PlayerPawn->GetActorLocation() - GetActorLocation();
    To.Z = 0.f;
    if (To.IsNearlyZero()) return;

    const FRotator Want = To.Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), Want, DeltaTime, 8.f));
}

FVector AEnemy::RandomReachablePoint(const FVector& Center, float Radius) const
{
    // On a NavMesh: a random point it can actually walk to (not inside a wall or outside the building)
    if (bUseNavMesh)
    {
        if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            FNavLocation Point;
            if (Nav->GetDefaultNavDataInstance() &&
                Nav->GetRandomReachablePointInRadius(Center, Radius, Point))
            {
                return Point.Location;
            }
        }
    }
    const FVector2D R = FMath::RandPointInCircle(Radius);
    return Center + FVector(R.X, R.Y, 0.f);
}

FVector AEnemy::PickNewPatrolPoint() const
{
    return RandomReachablePoint(SpawnLocation, PatrolRadius);
}

// ------------------------------------------------------------------ footsteps

void AEnemy::UpdateFootsteps(float DeltaTime)
{
    if (!bPlayFootsteps) return;
    const UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move || !Move->IsMovingOnGround()) return;

    const float Speed = static_cast<float>(GetVelocity().Size2D());
    if (Speed < 30.f)
    {
        return;
    }

    // Longer strides the faster it goes
    StepDistance += Speed * DeltaTime;
    const float Stride = FMath::Lerp(70.f, 125.f, FMath::Clamp((Speed - 150.f) / 450.f, 0.f, 1.f));
    if (StepDistance < Stride)
    {
        return;
    }
    StepDistance = 0.f;

    const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
    const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
    const AHRealism::ESurface Surface = AHRealism::SurfaceUnder(GetWorld(), Feet, 40.f, this);
    const AHRealism::FSurfaceInfo& Info = AHRealism::Info(Surface);
    USoundBase* Sound = AHRealism::RandomVariant(Info.FootstepSound, Info.FootstepVariants);
    if (!Sound)
    {
        Sound = AHRealism::RandomVariant(TEXT("SW_Footstep_Concrete"), 4);
    }
    const float Volume = FootstepVolume * FMath::Clamp(Speed / 450.f, 0.3f, 1.2f) * FMath::Clamp(Info.FootstepLoudness, 0.5f, 1.5f);
    AHRealism::PlayAt(this, Sound, Feet, Volume, FootstepPitch * FMath::FRandRange(0.93f, 1.07f), FootstepAudibleRange);
}

void AEnemy::DrawDebugSenses() const
{
    UWorld* World = GetWorld();
    const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const bool bAlert = (State == EAIState::Chase || State == EAIState::Attack);
    const float HalfAngle = FMath::DegreesToRadians(bAlert ? AlertVisionHalfAngle : VisionHalfAngle);

    float Visibility = 1.f;
    if (const AMyFPSPawn* Player = Cast<AMyFPSPawn>(PlayerPawn))
    {
        Visibility = bUseLighting ? Player->GetVisibility01() : 1.f;
    }
    const float Range = FMath::Lerp(DarkSightRange, DetectionRadius, Visibility);

    const FColor Colour = bAlert ? FColor::Red : (State == EAIState::Search ? FColor::Orange : FColor::Cyan);
    DrawDebugCone(World, Eye, GetActorForwardVector(), Range, HalfAngle, HalfAngle, 16, Colour, false, -1.f, 0, 1.f);
    DrawDebugCircle(World, GetActorLocation(), CloseSenseRadius, 24, Colour, false, -1.f, 0, 1.f,
        FVector(1, 0, 0), FVector(0, 1, 0), false);

    static const TCHAR* StateNames[] = { TEXT("Patrol"), TEXT("Chase"), TEXT("Attack"), TEXT("Search"), TEXT("Dead") };
    const int32 S = FMath::Clamp(static_cast<int32>(State), 0, 4);
    DrawDebugString(World, Eye + FVector(0, 0, 40), FString::Printf(TEXT("%s  %.0f%%"), StateNames[S], Awareness * 100.f),
        nullptr, Colour, 0.f, true, 1.1f);
    if (State == EAIState::Search)
    {
        DrawDebugSphere(World, SearchCenter, 40.f, 8, FColor::Orange, false, -1.f);
    }
}

void AEnemy::PerformAttack()
{
    if (!PlayerPawn) return;

    UGameplayStatics::ApplyDamage(PlayerPawn, AttackDamage, GetController(), this, UDamageType::StaticClass());
    UE_LOG(LogTemp, Log, TEXT("%s hit the player for %.1f"), *GetName(), AttackDamage);
}

// ------------------------------------------------------------------ damage / death

float AEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (bDead) return 0.f;

    const float Applied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);
    OnDamaged(DamageAmount);

    // Getting hurt while not hunting = you know where the player is now
    if (State == EAIState::Patrol || State == EAIState::Search)
    {
        if (PlayerPawn)
        {
            LastSeenPlayerLocation = PlayerPawn->GetActorLocation();
            LastSeenTime = GetWorld()->GetTimeSeconds();
        }
        Awareness = 1.f;
        SwitchState(EAIState::Chase);
    }

    if (CurrentHealth <= 0.f)
    {
        OnDeath();
    }

    return Applied;
}

void AEnemy::OnDeath()
{
    if (bDead) return;
    bDead = true;
    State = EAIState::Dead;

    // Stop all movement immediately
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->DisableMovement();
        Move->StopMovementImmediately();
    }

    // Release AI controller
    if (AController* C = GetController())
        C->UnPossess();

    // Disable capsule so ragdoll doesn't block player (test T10 fix)
    if (UCapsuleComponent* Cap = GetCapsuleComponent())
        Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Play the dying animation first if there is one, then ragdoll
    float RagdollDelay = 0.f;
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        if (UAnimationAsset* Anim = DyingAnim.LoadSynchronous())
        {
            Skel->PlayAnimation(Anim, false);
            RagdollDelay = FMath::Clamp(Anim->GetPlayLength() * 0.8f, 0.1f, 3.f);
        }
    }

    if (RagdollDelay > 0.f)
    {
        GetWorldTimerManager().SetTimer(TimerHandle_Ragdoll, this, &AEnemy::EnableRagdoll, RagdollDelay, false);
    }
    else
    {
        EnableRagdoll();
    }

    SetLifeSpan(12.f);  // cleanup after 12s
}

void AEnemy::EnableRagdoll()
{
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        Skel->SetCollisionProfileName(TEXT("Ragdoll"));
        Skel->SetAllBodiesSimulatePhysics(true);
        Skel->SetSimulatePhysics(true);
        Skel->WakeAllRigidBodies();
        Skel->bBlendPhysics = true;
    }
}
