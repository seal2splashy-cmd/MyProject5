// AEnemy.cpp
// [Rebuilt Oct 2026 from coursework excerpts]

#include "AEnemy.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
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

    // No skeletal mesh assigned yet? Show the capsule so you can still see and test it.
    if (!GetMesh() || !GetMesh()->GetSkeletalMeshAsset())
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

    switch (State)
    {
    case EAIState::Patrol:
        CurrentPatrolTarget = PickNewPatrolPoint();
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

    if (FVector::Dist2D(GetActorLocation(), CurrentPatrolTarget) <= PatrolAcceptRadius)
    {
        CurrentPatrolTarget = PickNewPatrolPoint();
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

    if (FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation()) <= AttackRange)
    {
        SwitchState(EAIState::Attack);
    }
}

void AEnemy::HandleState_Attack(float DeltaTime)
{
    if (!PlayerPawn || !CanDetectPlayer(true))
    {
        SwitchState(EAIState::Patrol);
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
        bool bVisible = true;
        if (bUseLineOfSight)
        {
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(AEnemyLOS), true, this);
            Params.AddIgnoredActor(this);
            Params.AddIgnoredActor(PlayerPawn);
            const FVector EyeLoc = MyLoc + FVector(0.f, 0.f, EyeHeight);
            const bool bHit = GetWorld()->LineTraceSingleByChannel(
                Hit, EyeLoc, PlayerLoc, ECC_Visibility, Params);
            bVisible = !bHit;
        }
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
    FVector ToTarget = Target - GetActorLocation();
    ToTarget.Z = 0.f;
    const float Dist = ToTarget.Size();

    if (Dist <= StopDist || Dist <= KINDA_SMALL_NUMBER)
    {
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
            Move->Velocity = FMath::VInterpTo(Move->Velocity, FVector::ZeroVector, DeltaTime, 8.f);
        return;
    }

    const FVector Dir = ToTarget / Dist;
    const float InputScale = FMath::Clamp((Dist - StopDist) / 600.f, 0.25f, 1.0f) * SpeedScale;
    AddMovementInput(Dir, InputScale);

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
        Move->MaxWalkSpeed = MoveSpeed * SpeedScale;
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
