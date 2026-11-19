// AEnemy.h
// Base enemy: hand-written 4-state FSM (Patrol / Chase / Attack / Dead), layered
// detection with line of sight, predictive chase with a self-correcting bias,
// NavMesh path following, point damage, and ragdoll death.
// [Rebuilt Oct 2026 from coursework excerpts - same design, names and tuning as the original]

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AEnemy.generated.h"

class UAnimationAsset;

UENUM(BlueprintType)
enum class EAIState : uint8
{
    Patrol,
    Chase,
    Attack,
    Dead
};

UCLASS()
class MYPROJECT5_API AEnemy : public ACharacter
{
    GENERATED_BODY()

public:
    AEnemy();

    virtual void Tick(float DeltaTime) override;

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

    bool IsDead() const { return bDead; }
    EAIState GetState() const { return State; }

protected:
    virtual void BeginPlay() override;

    // ---- State machine ----
    void SwitchState(EAIState NewState);
    virtual void HandleState_Patrol(float DeltaTime);
    virtual void HandleState_Chase(float DeltaTime);
    virtual void HandleState_Attack(float DeltaTime);

    // ---- Helpers shared with ASmartEnemy ----
    bool CanDetectPlayer(bool bUpdateMemory = true);
    /** True if nothing solid is between the eyes and the player (their own gun/arms don't count). */
    bool HasLineOfSightToPlayer() const;
    void UpdatePlayerObservation(float DeltaTime);
    FVector PredictPlayerLocation();
    /** Walks towards Target: along a NavMesh path when the level has one, else in a straight line.
     *  Stops once the remaining distance (along the path) is within StopDist. */
    void MoveTowards(const FVector& Target, float DeltaTime, float StopDist, float SpeedScale = 1.f);
    void FacePlayer(float DeltaTime);
    FVector PickNewPatrolPoint() const;

    /** Called when an attack lands. Override to add rewards etc. */
    virtual void PerformAttack();
    /** Called whenever this enemy actually takes damage (after health is reduced). */
    virtual void OnDamaged(float Amount) {}
    virtual void OnDeath();

    // ---- Tuning ----
    UPROPERTY(EditAnywhere, Category = "Enemy|Health")
    float MaxHealth = 100.f;

    UPROPERTY(VisibleInstanceOnly, Category = "Enemy|Health")
    float CurrentHealth = 100.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Movement")
    float MoveSpeed = 450.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    float PatrolRadius = 1200.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    float PatrolAcceptRadius = 120.f;

    /** Seconds before it gives up on a patrol point it can't reach and picks another. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Patrol")
    float PatrolGiveUpTime = 15.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float DetectionRadius = 2500.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float LoseSightRadius = 3000.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float GiveUpTime = 4.f;

    /** Needs a clear view of the player to spot them (walls, crates and doors block it).
     *  Untick for the old radius-only detection. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    bool bUseLineOfSight = true;

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float EyeHeight = 60.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackRange = 160.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackDamage = 15.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Attack")
    float AttackCooldown = 1.2f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Prediction")
    float PredictionTime = 0.5f;

    /** How fast the prediction bias corrects towards its own error. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Prediction")
    float LearningRate = 1.5f;

    // ---- Pathfinding ----
    /** Follow NavMesh paths around walls. Without a NavMesh in the level (or unticked) it walks
     *  straight at its target like before. Add one with Tools/add_navmesh_ue58.py. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Pathfinding")
    bool bUseNavMesh = true;

    /** Re-plan when the target has moved this far since the last plan... */
    UPROPERTY(EditAnywhere, Category = "Enemy|Pathfinding", meta = (ClampMin = 10))
    float RepathDistance = 120.f;

    /** ...but not more often than this (seconds)... */
    UPROPERTY(EditAnywhere, Category = "Enemy|Pathfinding", meta = (ClampMin = 0.05))
    float MinRepathInterval = 0.25f;

    /** ...and always at least this often, in case something moved into the way. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Pathfinding", meta = (ClampMin = 0.1))
    float MaxRepathInterval = 1.5f;

    /** A path corner counts as reached within this distance. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Pathfinding", meta = (ClampMin = 10))
    float WaypointAcceptRadius = 60.f;

    /** Draws the current path in the world (green) - handy for testing evidence. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Pathfinding")
    bool bDrawDebugPath = false;

    /** Optional death animation (played before ragdoll takes over). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Animation")
    TSoftObjectPtr<UAnimationAsset> DyingAnim;

    // ---- Runtime ----
    UPROPERTY()
    APawn* PlayerPawn = nullptr;

    EAIState State = EAIState::Patrol;
    bool bDead = false;

    FVector SpawnLocation = FVector::ZeroVector;
    FVector CurrentPatrolTarget = FVector::ZeroVector;
    float PatrolTargetTime = 0.f;

    FVector LastSeenPlayerLocation = FVector::ZeroVector;
    float LastSeenTime = -1000.f;
    float LastAttackTime = -1000.f;

    FVector LastObservedPlayerPos = FVector::ZeroVector;
    FVector PlayerVelocityEstimate = FVector::ZeroVector;
    FVector PredictionBias = FVector::ZeroVector;
    FVector LastPrediction = FVector::ZeroVector;
    bool bHasObservation = false;

private:
    void EnableRagdoll();
    FTimerHandle TimerHandle_Ragdoll;

    // ---- Path following (see MoveTowards) ----
    /** Plans a path to Target into PathPoints. False = no NavMesh / no route (caller goes direct). */
    bool PlanPath(const FVector& Target);
    void ClearPath();
    /** Remaining distance along the path from MyLoc, via PathPoints[PathIndex]. */
    float RemainingPathLength(const FVector& MyLoc) const;

    TArray<FVector> PathPoints;
    int32 PathIndex = 0;
    FVector PathGoal = FVector::ZeroVector;
    float PathPlannedTime = -1000.f;
    bool bHasPath = false;
    bool bWarnedNoNavMesh = false;
};
