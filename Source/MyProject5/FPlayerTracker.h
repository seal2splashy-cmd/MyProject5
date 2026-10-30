// FPlayerTracker.h
// Turns continuous world state into one of 162 discrete states for the Q-table.
// 3 distance x 3 approach x 3 speed x 2 facing x 3 behaviour = 162 states.
// [Rebuilt Oct 2026 from coursework excerpts - same buckets and thresholds]

#pragma once

#include "CoreMinimal.h"

enum class EDistanceBucket  : uint8 { Close = 0, Mid = 1, Far = 2 };
enum class EApproachDir     : uint8 { Approaching = 0, Lateral = 1, Retreating = 2 };
enum class ESpeedBucket     : uint8 { Still = 0, Walk = 1, Run = 2 };
enum class EBehaviorPattern : uint8 { Aggressive = 0, Neutral = 1, Evasive = 2 };

struct FTrackedPlayerState
{
    EDistanceBucket  Distance  = EDistanceBucket::Far;
    EApproachDir     Approach  = EApproachDir::Lateral;
    ESpeedBucket     Speed     = ESpeedBucket::Still;
    bool             bLookingAtEnemy = false;
    EBehaviorPattern Behavior  = EBehaviorPattern::Neutral;

    /** Mixed-radix encode into [0, 161]. */
    int32 Encode() const
    {
        return (int32)Distance * 54
             + (int32)Approach * 18
             + (int32)Speed    * 6
             + (bLookingAtEnemy ? 1 : 0) * 3
             + (int32)Behavior;
    }
};

class MYPROJECT5_API FPlayerTracker
{
public:
    // Bucket boundaries (cm, cm/s, dot products)
    float CloseDist = 500.f;
    float MidDist   = 1500.f;
    float StillSpeed = 50.f;
    float WalkSpeed  = 600.f;
    float ApproachDot = 0.4f;
    float LookingDot  = 0.7f;
    float BehaviorDelta = 150.f;

    FTrackedPlayerState Sample(const FVector& EnemyLoc, const FVector& PlayerLoc,
        const FVector& PlayerVelocity, const FVector& PlayerForward);

    void Reset() { HistoryIndex = 0; HistoryCount = 0; }

private:
    // Circular buffer for Aggressive/Neutral/Evasive classification
    static constexpr int32 HistorySize = 8;
    float DistanceHistory[HistorySize] = {};
    int32 HistoryIndex = 0;
    int32 HistoryCount = 0;
};
