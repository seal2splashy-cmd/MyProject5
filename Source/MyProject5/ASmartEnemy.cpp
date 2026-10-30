// ASmartEnemy.cpp
// [Rebuilt Oct 2026 from coursework excerpts]

#include "ASmartEnemy.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/Paths.h"

ASmartEnemy::ASmartEnemy()
{
    // Slightly tougher than the base enemy
    MaxHealth = 120.f;
}

FString ASmartEnemy::GetQTablePath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QLearning"), TEXT("qtable.json"));
}

void ASmartEnemy::BeginPlay()
{
    Super::BeginPlay();

    QLAgent.Alpha   = Alpha;
    QLAgent.Gamma   = Gamma;
    QLAgent.Epsilon = Epsilon;

    if (bPersistQTable)
    {
        QLAgent.LoadQTable(GetQTablePath());
        UE_LOG(LogTemp, Log, TEXT("SmartEnemy: loaded %d Q-table entries"), QLAgent.NumEntries());
    }

    CircleDir = FMath::RandBool() ? 1.f : -1.f;
}

void ASmartEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bPersistQTable)
    {
        QLAgent.SaveQTable(GetQTablePath());
    }
    Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------ the loop

void ASmartEnemy::MakeDecision()
{
    if (!PlayerPawn) return;

    // 1. Sample current state
    const FTrackedPlayerState Tracked = Tracker.Sample(
        GetActorLocation(),
        PlayerPawn->GetActorLocation(),
        PlayerVelocityEstimate,
        PlayerPawn->GetActorForwardVector());
    const int32 CurrentState = Tracked.Encode();

    // 2. Update Q-table from the previous action (skip the very first tick)
    if (LastState >= 0)
    {
        const float Reward = ComputeAndResetReward();
        QLAgent.UpdateQ(LastState, LastAction, Reward, CurrentState);

        if (bLogDecisions)
        {
            UE_LOG(LogTemp, Log, TEXT("SmartEnemy: s=%d a=%s r=%.2f -> s'=%d"),
                LastState, FQLearningAgent::ActionName(LastAction), Reward, CurrentState);
        }
    }

    // 3. Select next action
    LastAction = QLAgent.SelectAction(CurrentState);
    LastState  = CurrentState;

    // 4. Distance snapshot for reward shaping
    LastDistToPlayer = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
}

float ASmartEnemy::ComputeAndResetReward()
{
    float Reward = PendingReward;   // damage dealt / taken accumulated since last decision
    PendingReward = 0.f;

    if (PlayerPawn)
    {
        const float NowDist = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());
        Reward += ((LastDistToPlayer - NowDist) / 100.f) * RewardCloseDistance;

        if (!CanDetectPlayer(false))
        {
            Reward += PenaltyLostSight;
        }
    }
    return Reward;
}

// ------------------------------------------------------------------ states

void ASmartEnemy::HandleState_Chase(float DeltaTime)
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
        // lost them: that last action earned the lost-sight penalty
        PendingReward += PenaltyLostSight;
        SwitchState(EAIState::Patrol);
        return;
    }

    DecisionTimer -= DeltaTime;
    if (DecisionTimer <= 0.f || LastState < 0)
    {
        DecisionTimer = DecisionInterval;
        MakeDecision();
    }

    ExecuteAction(LastAction, DeltaTime);

    if (FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation()) <= AttackRange)
    {
        SwitchState(EAIState::Attack);
    }
}

void ASmartEnemy::HandleState_Attack(float DeltaTime)
{
    // Keep learning while in melee range too - the base handler deals the damage
    DecisionTimer -= DeltaTime;
    if (DecisionTimer <= 0.f)
    {
        DecisionTimer = DecisionInterval;
        MakeDecision();
    }

    // Retreat is a legitimate learned choice even at close range
    if (LastAction == ESmartAction::Retreat)
    {
        ExecuteAction(LastAction, DeltaTime);
        SwitchState(EAIState::Chase);
        return;
    }

    Super::HandleState_Attack(DeltaTime);
}

void ASmartEnemy::ExecuteAction(ESmartAction Action, float DeltaTime)
{
    if (!PlayerPawn) return;

    const FVector MyLoc = GetActorLocation();
    const FVector PlayerLoc = PredictPlayerLocation();

    FVector ToPlayer = PlayerLoc - MyLoc;
    ToPlayer.Z = 0.f;
    ToPlayer = ToPlayer.GetSafeNormal();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, ToPlayer);

    switch (Action)
    {
    case ESmartAction::Rush:
        MoveTowards(PlayerLoc, DeltaTime, AttackRange * 0.5f, 1.4f);
        break;

    case ESmartAction::FlankLeft:
        MoveTowards(PlayerLoc - Right * FlankOffset, DeltaTime, 80.f, 1.0f);
        break;

    case ESmartAction::FlankRight:
        MoveTowards(PlayerLoc + Right * FlankOffset, DeltaTime, 80.f, 1.0f);
        break;

    case ESmartAction::Retreat:
        MoveTowards(MyLoc - ToPlayer * RetreatDistance, DeltaTime, 80.f, 1.0f);
        break;

    case ESmartAction::Circle:
    {
        // Orbit the player at CircleRadius, tangential to the line between us
        const FVector Radial = -ToPlayer * CircleRadius;
        const FVector Tangent = Right * CircleDir * 400.f;
        MoveTowards(PlayerLoc + Radial + Tangent, DeltaTime, 60.f, 0.9f);
        break;
    }

    case ESmartAction::HoldPosition:
        MoveTowards(MyLoc, DeltaTime, 1000000.f, 0.f);   // brake
        FacePlayer(DeltaTime);
        break;
    }
}

// ------------------------------------------------------------------ rewards from events

void ASmartEnemy::PerformAttack()
{
    Super::PerformAttack();
    PendingReward += RewardDamagePlayer;
}

void ASmartEnemy::OnDamaged(float Amount)
{
    Super::OnDamaged(Amount);
    PendingReward += PenaltyTakingDamage;
}

void ASmartEnemy::OnDeath()
{
    // Make sure the action that got us killed is punished before we stop learning
    if (LastState >= 0)
    {
        QLAgent.UpdateQ(LastState, LastAction, ComputeAndResetReward(), LastState);
    }
    if (bPersistQTable)
    {
        QLAgent.SaveQTable(GetQTablePath());
    }
    Super::OnDeath();
}
