// AHSettingsSave.h
// The player's settings, saved to Saved/SaveGames/AHSettings.sav (survives restarts and builds).
// AAHPlayerController loads them, applies them to the pawn / lens / audio, and saves on every change.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AHSettingsSave.generated.h"

UCLASS()
class MYPROJECT5_API UAHSettingsSave : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY()
    float MouseSensitivity = 1.f;

    UPROPERTY()
    bool bInvertY = false;

    UPROPERTY()
    float FieldOfView = 100.f;

    /** 0..1 */
    UPROPERTY()
    float MasterVolume = 1.f;

    /** Exposure compensation (EV stops). */
    UPROPERTY()
    float Brightness = 0.f;

    /** 0 off, 1 reduced, 2 full bodycam glitches / rolling shutter. */
    UPROPERTY()
    int32 BodycamEffects = 2;

    UPROPERTY()
    bool bMotionBlur = true;

    static UAHSettingsSave* LoadOrCreate();
    void Save();
};
