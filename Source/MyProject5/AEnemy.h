// AEnemy.h
// Base enemy: hand-written 4-state FSM (Patrol / Chase / Attack / Dead), layered
// detection with optional line of sight, predictive chase with a self-correcting
// bias, point damage, and ragdoll death.
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
    void UpdatePlayerObservation(float DeltaTime);
    FVector PredictPlayerLocation();
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

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float DetectionRadius = 2500.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float LoseSightRadius = 3000.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float GiveUpTime = 4.f;

    /** Off by default (documented limitation) - tick it to require real line of sight. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    bool bUseLineOfSight = false;

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
};
