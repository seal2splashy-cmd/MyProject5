#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MyHUD.generated.h"

/**
 * Minimal bodycam HUD:
 *  - dynamic crosshair that follows the gun muzzle's projected aim point (screen centre fallback)
 *  - health bar (green > 60%, amber 30-60%, red < 30%) and stamina bar, bottom left
 *  - ammo readout, death screen
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

    void DrawCrosshairAt(float ScreenX, float ScreenY);
    void DrawHealthBar(float Health, float MaxHealth);
    void DrawStaminaBar(float Stamina, float MaxStamina);
    void DrawDeathScreen(float TimeSinceDeath);
};
