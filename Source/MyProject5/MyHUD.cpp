#include "MyHUD.h"
#include "MyFPSPawn.h"
#include "AGun.h"
#include "BodycamTorchComponent.h"
#include "BodycamLensComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Camera/PlayerCameraManager.h"

void AMyHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!Canvas) return;

    float ScreenX = Canvas->SizeX * 0.5f;
    float ScreenY = Canvas->SizeY * 0.5f;
    bool bDrawCrosshair = true;
    CrosshairGapPx = 0.f;
    CrosshairAlpha = 1.f;

    if (bShowBodycamOverlay)
    {
        DrawBodycamOverlay();
    }

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
                        // Follow the bodycam lens warp so the crosshair sits on what the gun points at
                        if (const UBodycamLensComponent* Lens = FPSPawn->GetBodycamLens())
                        {
                            ScreenPos = Lens->DistortScreenPoint(ScreenPos, FVector2D(Canvas->SizeX, Canvas->SizeY));
                        }
                        ScreenX = ScreenPos.X;
                        ScreenY = ScreenPos.Y;
                    }

                    // Open the crosshair to the gun's real accuracy cone; fade it out as the sights come up
                    float FOV = 90.f;
                    if (PC->PlayerCameraManager)
                    {
                        FOV = PC->PlayerCameraManager->GetFOVAngle();
                    }
                    const float PxPerDeg = Canvas->SizeX / FMath::Max(FOV, 1.f);
                    CrosshairGapPx = Gun->GetCurrentSpreadDeg() * PxPerDeg;
                    CrosshairAlpha = 1.f - FMath::Clamp((Gun->GetAimAlpha() - 0.5f) * 2.f, 0.f, 1.f);

                    // Ammo, bottom right
                    const FString Ammo = Gun->IsReloading()
                        ? FString(TEXT("RELOADING"))
                        : FString::Printf(TEXT("%d | %d  (%d mags)"), Gun->GetAmmoInMag(), Gun->GetReserveAmmo(), Gun->GetSpareMagCount());
                    DrawText(Ammo, FLinearColor(1.f, 1.f, 1.f, 0.8f),
                        Canvas->SizeX - 230.f, Canvas->SizeY - 64.f, GEngine ? GEngine->GetMediumFont() : nullptr);
                }
            }

            DrawHealthBar(FPSPawn->GetHealth(), FPSPawn->GetMaxHealth());
            DrawStaminaBar(FPSPawn->GetStamina(), FPSPawn->GetMaxStamina());
            if (const UBodycamTorchComponent* Torch = FPSPawn->GetTorch())
            {
                DrawTorchBattery(Torch->GetBattery01(), Torch->IsTorchOn(), Torch->IsBatteryLow());
            }
        }
    }

    if (bDrawCrosshair && CrosshairAlpha > 0.01f)
    {
        DrawCrosshairAt(ScreenX, ScreenY);
    }

    // What F will do
    const AMyFPSPawn* ViewPawn = PC ? Cast<AMyFPSPawn>(PC->GetPawn()) : nullptr;
    if (ViewPawn && !ViewPawn->IsDead())
    {
        const FString Prompt = ViewPawn->GetInteractPrompt();
        if (!Prompt.IsEmpty())
        {
            const FString Line = TEXT("[F]  ") + Prompt;
            UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
            float W = 0.f, H = 0.f;
            GetTextSize(Line, W, H, Font, 1.f);
            const float X = Canvas->SizeX * 0.5f - W * 0.5f;
            const float Y = Canvas->SizeY * 0.5f + 70.f;
            DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 10.f, Y - 4.f, W + 20.f, H + 8.f);
            DrawText(Line, FLinearColor(0.95f, 0.95f, 0.9f, 0.95f), X, Y, Font, 1.f);
        }
        if (ViewPawn->IsHealing())
        {
            DrawText(TEXT("Bandaging..."), FLinearColor(0.85f, 0.85f, 0.8f, 0.85f), 32.f, Canvas->SizeY - 150.f,
                GEngine ? GEngine->GetSmallFont() : nullptr, 1.2f);
        }
    }
}

void AMyHUD::DrawBodycamOverlay()
{
    // What real body-worn cameras burn into the corner of their footage
    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
    const FDateTime Now = FDateTime::Now();
    const FString Stamp = FString::Printf(TEXT("%04d-%02d-%02d  %02d:%02d:%02d"),
        Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond());

    const float Right = Canvas->SizeX - 24.f;
    float W = 0.f, H = 0.f;
    GetTextSize(Stamp, W, H, Font, 1.2f);
    const float X = Right - W;
    const float Y = 22.f;

    const FLinearColor Text(0.95f, 0.95f, 0.92f, 0.85f);
    DrawText(Stamp, Text, X, Y, Font, 1.2f);
    DrawText(CameraUnitId, Text, X, Y + H + 2.f, Font, 1.2f);

    // REC with a blinking red dot
    const bool bOn = FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.0) < 0.6;
    float RW = 0.f, RH = 0.f;
    GetTextSize(TEXT("REC"), RW, RH, Font, 1.2f);
    const float RecX = X - RW - 26.f;
    if (bOn)
    {
        DrawRect(FLinearColor(0.9f, 0.05f, 0.05f, 0.9f), RecX - 14.f, Y + RH * 0.5f - 4.f, 8.f, 8.f);
    }
    DrawText(TEXT("REC"), Text, RecX, Y, Font, 1.2f);
}

void AMyHUD::DrawCrosshairAt(float X, float Y)
{
    if (!Canvas) return;

    const float Inner = LineGap + CrosshairGapPx;
    const float Outer = Inner + LineLength;
    const FLinearColor OutlineColorNow(OutlineColor.R, OutlineColor.G, OutlineColor.B, OutlineColor.A * CrosshairAlpha);
    const FLinearColor CrosshairColorNow(CrosshairColor.R, CrosshairColor.G, CrosshairColor.B, CrosshairColor.A * CrosshairAlpha);

    // Outline pass
    DrawLine(X - Outer, Y, X - Inner, Y, OutlineColorNow, OutlineThickness);
    DrawLine(X + Inner, Y, X + Outer, Y, OutlineColorNow, OutlineThickness);
    DrawLine(X, Y - Outer, X, Y - Inner, OutlineColorNow, OutlineThickness);
    DrawLine(X, Y + Inner, X, Y + Outer, OutlineColorNow, OutlineThickness);

    // Fill pass
    DrawLine(X - Outer, Y, X - Inner, Y, CrosshairColorNow, LineThickness);
    DrawLine(X + Inner, Y, X + Outer, Y, CrosshairColorNow, LineThickness);
    DrawLine(X, Y - Outer, X, Y - Inner, CrosshairColorNow, LineThickness);
    DrawLine(X, Y + Inner, X, Y + Outer, CrosshairColorNow, LineThickness);

    // Centre dot outline
    DrawRect(OutlineColorNow,
        X - DotRadius - 1.f, Y - DotRadius - 1.f,
        (DotRadius + 1.f) * 2.f, (DotRadius + 1.f) * 2.f);
    // Centre dot fill
    DrawRect(CrosshairColorNow,
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

void AMyHUD::DrawTorchBattery(float Battery01, bool bTorchOn, bool bLow)
{
    // Thin amber bar under stamina; hidden while full and off so the HUD stays clean.
    if (!bTorchOn && Battery01 >= 0.999f)
    {
        return;
    }

    const float Pct = FMath::Clamp(Battery01, 0.f, 1.f);
    const float BarW = 240.f;
    const float BarH = 4.f;
    const float X = 40.f;
    const float Y = Canvas->SizeY - 28.f;

    FLinearColor Fill(1.f, 0.72f, 0.2f, bTorchOn ? 0.85f : 0.45f);
    if (bLow)
    {
        // Blink red when low
        const bool bBlink = FMath::Fmod(GetWorld()->GetTimeSeconds(), 0.8) < 0.4;
        Fill = FLinearColor(0.95f, 0.15f, 0.1f, bBlink ? 0.9f : 0.35f);
    }

    DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f), X, Y, BarW, BarH);
    DrawRect(Fill, X, Y, BarW * Pct, BarH);
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
