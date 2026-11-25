// AHMissionProgress.cpp

#include "AHMissionProgress.h"

void UAHMissionProgress::SaveCheckpoint(const FString& InLevelName, int32 InStage, const FVector& InLocation,
    float InYaw, float InElapsed)
{
    LevelName = InLevelName;
    Stage = InStage;
    Location = InLocation;
    Yaw = InYaw;
    Elapsed = InElapsed;
    bValid = true;
}

void UAHMissionProgress::Clear()
{
    LevelName.Reset();
    Stage = 0;
    Location = FVector::ZeroVector;
    Yaw = 0.f;
    Elapsed = 0.f;
    bValid = false;
    Deaths = 0;
}

bool UAHMissionProgress::GetCheckpoint(const FString& InLevelName, int32& OutStage, FVector& OutLocation,
    float& OutYaw, float& OutElapsed) const
{
    if (!bValid || LevelName != InLevelName)
    {
        return false;
    }
    OutStage = Stage;
    OutLocation = Location;
    OutYaw = Yaw;
    OutElapsed = Elapsed;
    return true;
}
