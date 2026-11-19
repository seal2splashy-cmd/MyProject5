// AEnemy.h
// Base enemy: hand-written FSM (Patrol / Chase / Attack / Search / Dead), senses that work
// like a person's (vision cone, needs light to see far, an awareness meter that takes time to
// fill, hearing through walls), predictive chase with a self-correcting bias, NavMesh path
// following, point damage, and ragdoll death.
// [Rebuilt Oct 2026 from coursework excerpts - same design, names and tuning as the original;
//  Search state, senses and hearing added Nov 2026 - Docs/REALISM.md]

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
    /** Heard something / half-saw something / lost the player: go and look around there. */
    Search,
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
    /** 0 = hasn't noticed anything, 1 = knows exactly where you are. */
    float GetAwareness() const { return Awareness; }

    /**
     * Something made a noise. Every enemy within Range (halved-ish through walls) hears it and
     * comes to look. Gunshots ~45 m, sprinting ~11 m, walking ~4.5 m, crouching silent, bullet
     * impacts / melee hits / landing / heavy breathing in between. `ah.DebugNoise 1` shows them.
     */
    static void ReportNoise(const UObject* WorldContext, const FVector& Location, float Range, AActor* NoiseMaker = nullptr);

    /** Called by ReportNoise. */
    void HearNoise(const FVector& Location, float Range, AActor* NoiseMaker);

protected:
    virtual void BeginPlay() override;

    // ---- State machine ----
    void SwitchState(EAIState NewState);
    virtual void HandleState_Patrol(float DeltaTime);
    virtual void HandleState_Chase(float DeltaTime);
    virtual void HandleState_Attack(float DeltaTime);
    virtual void HandleState_Search(float DeltaTime);

    /** Go to Center and look around it for SearchDuration. Urgent = hurry there (gunshot, lost a target). */
    void BeginSearch(const FVector& Center, bool bUrgent);

    // ---- Helpers shared with ASmartEnemy ----
    /** In Chase/Attack: can it perceive the player right now (and remember where they are)? */
    bool CanDetectPlayer(bool bUpdateMemory = true);
    /**
     * Raw eyesight this frame: within range for the light the player is in, inside the vision
     * cone (wider when alert, any direction when right next to it or dazzled by their torch),
     * and nothing solid in the way. OutCloseness: 1 right next to it, 0 at the edge of sight.
     */
    bool SeesPlayer(float* OutCloseness = nullptr) const;
    /** True if nothing solid is between the eyes and the player's head or chest (their own gun/arms don't count). */
    bool HasLineOfSightToPlayer() const;
    /** Patrol/Search: fill or drain the awareness meter; switches to Search/Chase. True if it switched state. */
    bool UpdateAwareness(float DeltaTime);
    void UpdatePlayerObservation(float DeltaTime);
    FVector PredictPlayerLocation();
    /** Walks towards Target: along a NavMesh path when the level has one, else in a straight line.
     *  Stops once the remaining distance (along the path) is within StopDist. */
    void MoveTowards(const FVector& Target, float DeltaTime, float StopDist, float SpeedScale = 1.f);
    void FacePlayer(float DeltaTime);
    FVector PickNewPatrolPoint() const;
    /** Random point it can walk to within Radius of Center (NavMesh), or a random point in the circle. */
    FVector RandomReachablePoint(const FVector& Center, float Radius) const;

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

    // ---- Eyes ----
    /** How far it can see you when you're well lit (or your torch is on). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float DetectionRadius = 2500.f;

    /** How far it can see you in the dark (no light on you, torch off). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float DarkSightRange = 700.f;

    /** Uses the light the player is standing in (AMyFPSPawn::GetVisibility01). Off = always DetectionRadius. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    bool bUseLighting = true;

    /** Half-angle of the vision cone while patrolling / searching (degrees). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection", meta = (ClampMin = 10, ClampMax = 180))
    float VisionHalfAngle = 60.f;

    /** Half-angle once it's hunting you (looking about more). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection", meta = (ClampMin = 10, ClampMax = 180))
    float AlertVisionHalfAngle = 90.f;

    /** Closer than this it notices you whichever way it's facing. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection")
    float CloseSenseRadius = 220.f;

    /** Awareness per second when it can see you (x up to 2 when you're close). 1 = spotted. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection", meta = (ClampMin = 0.1))
    float AwarenessGain = 1.8f;

    /** Awareness lost per second when it can't see you. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection", meta = (ClampMin = 0.0))
    float AwarenessDecay = 0.3f;

    /** Awareness at which a patrolling enemy stops and comes to check ("what was that?"). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Detection", meta = (ClampMin = 0.05, ClampMax = 0.95))
    float SuspicionLevel = 0.4f;

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

    // ---- Ears ----
    /** x every noise's range. 0 = deaf. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Hearing", meta = (ClampMin = 0.0))
    float HearingScale = 1.f;

    /** Noise range through a wall (fraction). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Hearing", meta = (ClampMin = 0.0, ClampMax = 1.0))
    float WallMuffle = 0.45f;

    /** It can't pinpoint a sound exactly: error as a fraction of the distance to it. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Hearing", meta = (ClampMin = 0.0, ClampMax = 0.5))
    float NoiseLocationError = 0.1f;

    // ---- Searching ----
    UPROPERTY(EditAnywhere, Category = "Enemy|Search")
    float SearchDuration = 14.f;

    /** How far from the spot it pokes around. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Search")
    float SearchRadius = 550.f;

    UPROPERTY(EditAnywhere, Category = "Enemy|Search", meta = (ClampMin = 0.1, ClampMax = 1.5))
    float SearchSpeedScale = 0.55f;

    // ---- Attack ----
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

    // ---- Its own footsteps (so you can hear it coming, muffled through walls) ----
    UPROPERTY(EditAnywhere, Category = "Enemy|Audio")
    bool bPlayFootsteps = true;

    UPROPERTY(EditAnywhere, Category = "Enemy|Audio", meta = (ClampMin = 0.0))
    float FootstepVolume = 0.8f;

    /** Lower = heavier. */
    UPROPERTY(EditAnywhere, Category = "Enemy|Audio", meta = (ClampMin = 0.3, ClampMax = 2.0))
    float FootstepPitch = 0.85f;

    /** Distance it can be heard walking (cm). */
    UPROPERTY(EditAnywhere, Category = "Enemy|Audio")
    float FootstepAudibleRange = 2200.f;

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

    float Awareness = 0.f;

    // Search state
    FVector SearchCenter = FVector::ZeroVector;
    FVector SearchPoint = FVector::ZeroVector;
    bool bSearchUrgent = false;
    bool bSearchReachedCenter = false;
    float SearchEndTime = 0.f;
    float SearchLegStartTime = 0.f;
    float SearchLookUntil = 0.f;
    float SearchLookYaw = 0.f;
    float LastHeardTime = -1000.f;
    float LastHeardLoudness = 0.f;

private:
    void EnableRagdoll();
    FTimerHandle TimerHandle_Ragdoll;

    void UpdateFootsteps(float DeltaTime);
    void DrawDebugSenses() const;
    float StepDistance = 0.f;

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
