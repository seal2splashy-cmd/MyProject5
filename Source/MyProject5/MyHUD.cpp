#include "MyHUD.h"
#include "MyFPSPawn.h"
#include "AGun.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"

void AMyHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas) return;

    float ScreenX = Canvas->SizeX * 0.5f;
    float ScreenY = Canvas->SizeY * 0.5f;
    bool bDrawCrosshair = true;

    APlayerController* PC = GetOwningPlayerController();
    if (PC)
    {
        if (AMyFPSPawn* FPSPawn = Cast<AMyFPSPawn>(PC->GetPawn()))
        {
            if (FPSPawn->IsDead())
            {
                DrawDeathScreen(GetWorld()->GetTimeSeconds() - FPSPawn->GetDeathTime());
                return;
            }

            // No crosshair with melee - you aim with your body, like real life
            bDrawCrosshair = !FPSPawn->IsMeleeEquipped();

            if (!bDrawCrosshair)
            {
                const FString Label = FPSPawn->GetMeleeWeaponName().ToUpper() + TEXT("   [2] swap");
                DrawText(Label, FLinearColor(1.f, 1.f, 1.f, 0.7f),
                    Canvas->SizeX - 260.f, Canvas->SizeY - 64.f, GEngine ? GEngine->GetMediumFont() : nullptr);
            }

            if (AGun* Gun = FPSPawn->GetCurrentGun())
            {
                if (bDrawCrosshair)
                {
                    const FVector AimWorldPoint = Gun->GetMuzzleAimPoint(AimProjectionDistance);

                    FVector2D ScreenPos;
                    if (PC->ProjectWorldLocationToScreen(AimWorldPoint, ScreenPos, true))
                    {
                        ScreenX = ScreenPos.X;
                        ScreenY = ScreenPos.Y;
                    }

                    // Ammo, bottom right
                    const FString Ammo = Gun->IsReloading()
                        ? FString(TEXT("RELOADING"))
                        : FString::Printf(TEXT("%d / %d"), Gun->GetAmmoInMag(), Gun->GetReserveAmmo());
                    DrawText(Ammo, FLinearColor(1.f, 1.f, 1.f, 0.8f),
                        Canvas->SizeX - 160.f, Canvas->SizeY - 64.f, GEngine ? GEngine->GetMediumFont() : nullptr);
                }
            }

            DrawHealthBar(FPSPawn->GetHealth(), FPSPawn->GetMaxHealth());
            DrawStaminaBar(FPSPawn->GetStamina(), FPSPawn->GetMaxStamina());
        }
    }

    if (bDrawCrosshair)
    {
        DrawCrosshairAt(ScreenX, ScreenY);
    }
}

void AMyHUD::DrawCrosshairAt(float X, float Y)
{
    if (!Canvas) return;

    const float Inner = LineGap;
    const float Outer = LineGap + LineLength;

    // Outline pass
    DrawLine(X - Outer, Y, X - Inner, Y, OutlineColor, OutlineThickness);
    DrawLine(X + Inner, Y, X + Outer, Y, OutlineColor, OutlineThickness);
    DrawLine(X, Y - Outer, X, Y - Inner, OutlineColor, OutlineThickness);
    DrawLine(X, Y + Inner, X, Y + Outer, OutlineColor, OutlineThickness);

    // Fill pass
    DrawLine(X - Outer, Y, X - Inner, Y, CrosshairColor, LineThickness);
    DrawLine(X + Inner, Y, X + Outer, Y, CrosshairColor, LineThickness);
    DrawLine(X, Y - Outer, X, Y - Inner, CrosshairColor, LineThickness);
    DrawLine(X, Y + Inner, X, Y + Outer, CrosshairColor, LineThickness);

    // Centre dot outline
    DrawRect(OutlineColor,
        X - DotRadius - 1.f, Y - DotRadius - 1.f,
        (DotRadius + 1.f) * 2.f, (DotRadius + 1.f) * 2.f);
    // Centre dot fill
    DrawRect(CrosshairColor,
        X - DotRadius, Y - DotRadius,
        DotRadius * 2.f, DotRadius * 2.f);
}

void AMyHUD::DrawHealthBar(float Health, float MaxHealth)
{
    const float Pct = FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f);
    const float BarW = 240.f;
    const float BarH = 14.f;
    const float X = 40.f;
    const float Y = Canvas->SizeY - 60.f;

    FLinearColor FillColor;
    if (Pct > 0.6f)      FillColor = FLinearColor(0.1f, 0.8f, 0.1f);
    else if (Pct > 0.3f) FillColor = FLinearColor(0.9f, 0.7f, 0.1f);
    else                 FillColor = FLinearColor(0.9f, 0.1f, 0.1f);

    DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f), X, Y, BarW, BarH);
    DrawRect(FillColor, X, Y, BarW * Pct, BarH);
}

void AMyHUD::DrawStaminaBar(float Stamina, float MaxStamina)
{
    const float Pct = FMath::Clamp(Stamina / FMath::Max(MaxStamina, 1.f), 0.f, 1.f);
    const float BarW = 240.f;
    const float BarH = 6.f;
    const float X = 40.f;
    const float Y = Canvas->SizeY - 40.f;

    DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f), X, Y, BarW, BarH);
    DrawRect(FLinearColor(0.2f, 0.5f, 1.f, 0.85f), X, Y, BarW * Pct, BarH);
}

void AMyHUD::DrawDeathScreen(float TimeSinceDeath)
{
    // Fade to near-black over ~1.5 s
    const float Alpha = FMath::Clamp(TimeSinceDeath / 1.5f, 0.f, 0.9f);
    DrawRect(FLinearColor(0.f, 0.f, 0.f, Alpha), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

    if (TimeSinceDeath > 0.6f)
    {
        UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
        const FString Msg = TEXT("SIGNAL LOST");
        float W = 0.f, H = 0.f;
        GetTextSize(Msg, W, H, Font, 2.f);
        DrawText(Msg, FLinearColor(0.85f, 0.1f, 0.1f, 1.f),
            (Canvas->SizeX - W) * 0.5f, (Canvas->SizeY - H) * 0.5f, Font, 2.f);
    }
}
