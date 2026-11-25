// AHMissionProgress.h
// Checkpoints for the A-07 mission. Dying reloads the level (AMyFPSPawn::Respawn), which wipes every
// actor - this lives on the GameInstance, so it survives the reload. AFacilityMissionDirector saves
// here after each objective and reads it back in its first Tick: done steps stay done and you wake
// up where you finished the last one. A new Play-In-Editor session starts a new GameInstance, so
// testing always starts fresh.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AHMissionProgress.generated.h"

UCLASS()
class MYPROJECT5_API UAHMissionProgress : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Remember a checkpoint for this level. */
    void SaveCheckpoint(const FString& InLevelName, int32 InStage, const FVector& InLocation, float InYaw, float InElapsed);

    /** Forget everything (mission complete / restart). */
    void Clear();

    /** True (and fills the outs) if there's a checkpoint for this level. */
    bool GetCheckpoint(const FString& InLevelName, int32& OutStage, FVector& OutLocation, float& OutYaw, float& OutElapsed) const;

    /** How many times you've been sent back to a checkpoint this run. */
    int32 GetDeaths() const { return Deaths; }
    void AddDeath() { ++Deaths; }

private:
    FString LevelName;
    int32 Stage = 0;
    FVector Location = FVector::ZeroVector;
    float Yaw = 0.f;
    float Elapsed = 0.f;
    bool bValid = false;
    int32 Deaths = 0;
};
