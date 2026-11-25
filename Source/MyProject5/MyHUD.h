#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MyHUD.generated.h"

class AFacilityMissionDirector;

/**
 * Minimal bodycam HUD:
 *  - the body camera's own burn-in, top right: blinking REC, date/time, unit number
 *  - dynamic crosshair that follows the gun muzzle's projected aim point (screen centre fallback),
 *    opening up with the gun's current spread and fading out when you aim down the sights
 *  - health bar (green > 60%, amber 30-60%, red < 30%) and stamina bar, bottom left
 *  - ammo readout (rounds ready / in spare mags), death screen
 *  - A-07 mission (AFacilityMissionDirector in the level): objective top left, mission messages,
 *    the "extraction confirmed" screen
 */
UCLASS()
class MYPROJECT5_API AMyHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    UPROPERTY(EditAnywhere, Category = "Crosshair")
    float LineGap = 6.f;

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    float LineLength = 10.f;

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    float LineThickness = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    float DotRadius = 1.5f;

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    float AimProjectionDistance = 8000.f;

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    FLinearColor OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

    UPROPERTY(EditAnywhere, Category = "Crosshair")
    float OutlineThickness = 3.5f;

    /** The timestamp / REC overlay real body cameras burn into their footage. */
    UPROPERTY(EditAnywhere, Category = "Bodycam")
    bool bShowBodycamOverlay = true;

    UPROPERTY(EditAnywhere, Category = "Bodycam")
    FString CameraUnitId = TEXT("AH-BC 0417");

    float CrosshairGapPx = 0.f;     // extra gap from the gun's spread, this frame
    float CrosshairAlpha = 1.f;

    void DrawCrosshairAt(float ScreenX, float ScreenY);
    void DrawHealthBar(float Health, float MaxHealth);
    void DrawStaminaBar(float Stamina, float MaxStamina);
    void DrawTorchBattery(float Battery01, bool bTorchOn, bool bLow);
    void DrawDeathScreen(float TimeSinceDeath);
    void DrawBodycamOverlay();
    void DrawMission();

    TWeakObjectPtr<AFacilityMissionDirector> MissionDirector;
    float NextDirectorSearch = 0.f;
};
