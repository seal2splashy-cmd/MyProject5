// AEnemy.cpp
// [Rebuilt Oct 2026 from coursework excerpts]

#include "AEnemy.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

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
    case EAIState::Dead:   break;
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
    case EAIState::Dead:
        // No entry logic at present; slots here for future extension
        break;
    }
}

// ------------------------------------------------------------------ states

void AEnemy::HandleState_Patrol(float DeltaTime)
{
    if (CanDetectPlayer(true))
    {
        bHasObservation = false;
        SwitchState(EAIState::Chase);
        return;
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
    if (!bSeen && (Now - LastSeenTime) > GiveUpTime)
    {
        SwitchState(EAIState::Patrol);
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
        // Ducked behind something: go and look where they were (Chase gives up after GiveUpTime)
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

// ------------------------------------------------------------------ helpers

bool AEnemy::CanDetectPlayer(bool bUpdateMemory)
{
    if (!PlayerPawn) return false;

    const FVector MyLoc = GetActorLocation();
    const FVector PlayerLoc = PlayerPawn->GetActorLocation();
    const float Dist = FVector::Dist(MyLoc, PlayerLoc);
    const float Now = GetWorld()->GetTimeSeconds();

    if (Dist <= DetectionRadius)  // primary radius
    {
        const bool bVisible = !bUseLineOfSight || HasLineOfSightToPlayer();
        if (bVisible && bUpdateMemory)
        {
            LastSeenPlayerLocation = PlayerLoc;
            LastSeenTime = Now;
        }
        return bVisible;
    }

    // Secondary: awareness persists within LoseSightRadius if recently seen
    if (Dist <= LoseSightRadius && (Now - LastSeenTime) <= GiveUpTime)
        return true;

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

    // Two rays, to the player's head and chest: peeking over a crate still gets you seen.
    const FVector EyeLoc = GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const FVector PlayerLoc = PlayerPawn->GetActorLocation();
    for (const float Up : { 50.f, 0.f })
    {
        FHitResult Hit;
        const FVector To = PlayerLoc + FVector(0.f, 0.f, Up);
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

void AEnemy::MoveTowards(const FVector& Target, float DeltaTime, float StopDist, float SpeedScale)
{
    const FVector MyLoc = GetActorLocation();
    UCharacterMovementComponent* Move = GetCharacterMovement();

    if (SpeedScale <= 0.f)   // "stand still" (Q-learning HoldPosition): just brake
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

    // Flank / retreat / circle targets can land inside a wall: snap them onto the NavMesh first.
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

FVector AEnemy::PickNewPatrolPoint() const
{
    // On a NavMesh: a random point it can actually walk to (not inside a wall or outside the building)
    if (bUseNavMesh)
    {
        if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        {
            FNavLocation Point;
            if (Nav->GetDefaultNavDataInstance() &&
                Nav->GetRandomReachablePointInRadius(SpawnLocation, PatrolRadius, Point))
            {
                return Point.Location;
            }
        }
    }
    const FVector2D R = FMath::RandPointInCircle(PatrolRadius);
    return SpawnLocation + FVector(R.X, R.Y, 0.f);
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

    // Getting shot while wandering = you know where the player is now
    if (State == EAIState::Patrol)
    {
        if (PlayerPawn)
        {
            LastSeenPlayerLocation = PlayerPawn->GetActorLocation();
            LastSeenTime = GetWorld()->GetTimeSeconds();
        }
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
