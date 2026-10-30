// ASmartEnemy.h
// AEnemy + Q-learning. Overrides Chase/Attack: every DecisionInterval it samples the
// player, rewards the last action, updates the Q-table and picks a new action.
// Q-table persists to Saved/QLearning/qtable.json between sessions.
// [Rebuilt Oct 2026 from coursework excerpts - same loop, rewards and hyperparameters]

#pragma once

#include "CoreMinimal.h"
#include "AEnemy.h"
#include "FQLearningAgent.h"
#include "FPlayerTracker.h"
#include "ASmartEnemy.generated.h"

UCLASS()
class MYPROJECT5_API ASmartEnemy : public AEnemy
{
    GENERATED_BODY()

public:
    ASmartEnemy();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void HandleState_Chase(float DeltaTime) override;
    virtual void HandleState_Attack(float DeltaTime) override;

    virtual void PerformAttack() override;
    virtual void OnDamaged(float Amount) override;
    virtual void OnDeath() override;

    // ---- Q-learning hyperparameters ----
    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Learning")
    float Alpha = 0.10f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Learning")
    float Gamma = 0.90f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Learning")
    float Epsilon = 0.15f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Learning")
    float DecisionInterval = 0.5f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Learning")
    bool bPersistQTable = true;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Learning")
    bool bLogDecisions = true;

    // ---- Rewards (rebalanced after test T14: was 5 / -5) ----
    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Rewards")
    float RewardDamagePlayer = 20.f;

    /** Per 100 cm of distance closed since the last decision. */
    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Rewards")
    float RewardCloseDistance = 1.f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Rewards")
    float PenaltyTakingDamage = -10.f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Rewards")
    float PenaltyLostSight = -2.f;

    // ---- Action shaping ----
    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Actions")
    float FlankOffset = 600.f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Actions")
    float RetreatDistance = 800.f;

    UPROPERTY(EditAnywhere, Category = "SmartEnemy|Actions")
    float CircleRadius = 500.f;

private:
    void MakeDecision();
    void ExecuteAction(ESmartAction Action, float DeltaTime);
    float ComputeAndResetReward();
    FString GetQTablePath() const;

    FQLearningAgent QLAgent;
    FPlayerTracker  Tracker;

    int32 LastState = -1;
    ESmartAction LastAction = ESmartAction::Rush;
    float PendingReward = 0.f;
    float LastDistToPlayer = 0.f;
    float DecisionTimer = 0.f;
    float CircleDir = 1.f;
};
