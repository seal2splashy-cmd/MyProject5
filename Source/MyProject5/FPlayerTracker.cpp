// FPlayerTracker.cpp
// [Rebuilt Oct 2026 from coursework excerpts]

#include "FPlayerTracker.h"

FTrackedPlayerState FPlayerTracker::Sample(const FVector& EnemyLoc, const FVector& PlayerLoc,
    const FVector& PlayerVelocity, const FVector& PlayerForward)
{
    FTrackedPlayerState Out;

    // Distance
    const float Dist = FVector::Dist(EnemyLoc, PlayerLoc);
    if      (Dist < CloseDist) Out.Distance = EDistanceBucket::Close;
    else if (Dist < MidDist)   Out.Distance = EDistanceBucket::Mid;
    else                       Out.Distance = EDistanceBucket::Far;

    const FVector ToEnemy = (EnemyLoc - PlayerLoc).GetSafeNormal();

    // Approach direction (only meaningful if the player is actually moving)
    const FVector FlatVel(PlayerVelocity.X, PlayerVelocity.Y, 0.f);
    const float Speed = FlatVel.Size();
    if (Speed > StillSpeed)
    {
        const float Dot = FVector::DotProduct(FlatVel / Speed, ToEnemy);
        if      (Dot >  ApproachDot) Out.Approach = EApproachDir::Approaching;
        else if (Dot < -ApproachDot) Out.Approach = EApproachDir::Retreating;
        else                         Out.Approach = EApproachDir::Lateral;
    }

    // Speed
    if      (Speed < StillSpeed) Out.Speed = ESpeedBucket::Still;
    else if (Speed < WalkSpeed)  Out.Speed = ESpeedBucket::Walk;
    else                         Out.Speed = ESpeedBucket::Run;

    // Is the player looking at us?
    Out.bLookingAtEnemy = FVector::DotProduct(PlayerForward.GetSafeNormal(), ToEnemy) > LookingDot;

    // Rolling history -> behaviour pattern
    DistanceHistory[HistoryIndex] = Dist;
    HistoryIndex = (HistoryIndex + 1) % HistorySize;   // wrap around
    if (HistoryCount < HistorySize) ++HistoryCount;

    if (HistoryCount >= 3)   // need a minimum of 3 samples for a trend
    {
        const int32 OldestIdx = (HistoryIndex - HistoryCount + HistorySize) % HistorySize;
        const float Delta = DistanceHistory[OldestIdx] - Dist;

        if      (Delta >  BehaviorDelta) Out.Behavior = EBehaviorPattern::Aggressive;   // getting closer
        else if (Delta < -BehaviorDelta) Out.Behavior = EBehaviorPattern::Evasive;      // moving away
        // else Neutral (default)
    }

    return Out;
}
