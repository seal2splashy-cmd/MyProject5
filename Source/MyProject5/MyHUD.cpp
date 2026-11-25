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
#include "FacilityMissionDirector.h"
#include "AHPlayerController.h"

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

    // Title / pause / settings take over the screen (the game is paused behind them)
    if (AAHPlayerController* MenuPC = Cast<AAHPlayerController>(GetOwningPlayerController()))
    {
        if (MenuPC->GetMenu() != EAHMenu::None)
        {
            DrawMenu(MenuPC);
            return;
        }
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

    DrawMission();
}

void AMyHUD::DrawMission()
{
    // Find the level's mission director (once a second until there is one: most maps have none)
    if (!MissionDirector.IsValid())
    {
        const float Now = GetWorld()->GetTimeSeconds();
        if (Now < NextDirectorSearch)
        {
            return;
        }
        NextDirectorSearch = Now + 1.f;
        MissionDirector = AFacilityMissionDirector::Get(this);
    }
    const AFacilityMissionDirector* Director = MissionDirector.Get();
    if (!Director || !Director->ShowsHUD())
    {
        return;
    }

    UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;
    UFont* Medium = GEngine ? GEngine->GetMediumFont() : nullptr;
    const float T = GetWorld()->GetTimeSeconds();

    // ---- the end screen
    if (Director->IsMissionComplete())
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);
        UFont* Large = GEngine ? GEngine->GetLargeFont() : nullptr;
        const int32 Secs = FMath::FloorToInt(Director->GetMissionTime());
        const FString Title = TEXT("EXTRACTION CONFIRMED");
        const FString Stats = FString::Printf(TEXT("Mission time  %02d:%02d      Checkpoints used  %d"),
            Secs / 60, Secs % 60, Director->GetDeaths());
        float W = 0.f, H = 0.f;
        GetTextSize(Title, W, H, Large, 2.f);
        const float Y = Canvas->SizeY * 0.42f;
        DrawText(Title, FLinearColor(0.4f, 1.f, 0.5f, 1.f), (Canvas->SizeX - W) * 0.5f, Y, Large, 2.f);
        float SW = 0.f, SH = 0.f;
        GetTextSize(Stats, SW, SH, Medium, 1.f);
        DrawText(Stats, FLinearColor(0.9f, 0.9f, 0.88f, 0.95f), (Canvas->SizeX - SW) * 0.5f, Y + H + 18.f, Medium, 1.f);
        const float RestartIn = Director->GetRestartIn();
        if (RestartIn >= 0.f)
        {
            const FString Again = FString::Printf(TEXT("Restarting in %d"), FMath::CeilToInt(RestartIn));
            float AW = 0.f, AH = 0.f;
            GetTextSize(Again, AW, AH, Small, 1.2f);
            DrawText(Again, FLinearColor(0.7f, 0.7f, 0.7f, 0.8f), (Canvas->SizeX - AW) * 0.5f, Y + H + SH + 40.f, Small, 1.2f);
        }
        return;
    }

    // ---- objective, top left, like a bodycam operator's tasking overlay
    {
        const float Age = Director->GetObjectiveAge();
        const bool bFresh = Age < 4.f;
        const float Flash = bFresh ? 0.5f + 0.5f * FMath::Cos(Age * 6.f) : 0.f;
        const FString Head = Director->IsBreached() ? TEXT("OBJECTIVE  //  ALARM") : TEXT("OBJECTIVE");
        const FString Body = Director->GetObjectiveText();
        float HW = 0.f, HH = 0.f, BW = 0.f, BH = 0.f;
        GetTextSize(Head, HW, HH, Small, 1.1f);
        GetTextSize(Body, BW, BH, Small, 1.25f);
        const float X = 28.f;
        const float Y = 24.f;
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.38f + 0.2f * Flash), X - 10.f, Y - 6.f, FMath::Max(HW, BW) + 20.f, HH + BH + 16.f);
        DrawRect(Director->IsBreached() ? FLinearColor(0.9f, 0.15f, 0.1f, 0.9f) : FLinearColor(1.f, 0.62f, 0.12f, 0.9f),
            X - 10.f, Y - 6.f, 3.f, HH + BH + 16.f);
        const bool bAlarmBlink = Director->IsBreached() && FMath::Fmod(T, 0.9f) < 0.45f;
        DrawText(Head, bAlarmBlink ? FLinearColor(1.f, 0.3f, 0.25f, 0.95f) : FLinearColor(1.f, 0.7f, 0.25f, 0.85f), X, Y, Small, 1.1f);
        DrawText(Body, FLinearColor(0.95f, 0.95f, 0.92f, 0.8f + 0.2f * Flash), X, Y + HH + 4.f, Small, 1.25f);
    }

    // ---- mission messages, lower middle
    FString MsgText;
    FLinearColor MsgColour = FLinearColor::White;
    float MsgAlpha = 0.f;
    if (Director->GetCurrentMessage(MsgText, MsgColour, MsgAlpha) && MsgAlpha > 0.01f)
    {
        float W = 0.f, H = 0.f;
        GetTextSize(MsgText, W, H, Medium, 1.f);
        const float X = (Canvas->SizeX - W) * 0.5f;
        const float Y = Canvas->SizeY * 0.30f;
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * MsgAlpha), X - 14.f, Y - 6.f, W + 28.f, H + 12.f);
        MsgColour.A = MsgAlpha;
        DrawText(MsgText, MsgColour, X, Y, Medium, 1.f);
    }
}

void AMyHUD::DrawBodycamOverlay()
{
    // What real body-worn cameras burn into the corner of their footage
    UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
    const UBodycamLensComponent* Lens = nullptr;
    if (const APlayerController* OwnerPC = GetOwningPlayerController())
    {
        if (const AMyFPSPawn* P = Cast<AMyFPSPawn>(OwnerPC->GetPawn()))
        {
            Lens = P->GetBodycamLens();
        }
    }
    const bool bHeld = Lens && Lens->IsFrameHeld();
    const float SignalLoss = Lens ? Lens->GetSignalLoss() : 0.f;

    const FDateTime Now = FDateTime::Now();
    if (!bHeld || LastStamp.IsEmpty())
    {
        LastStamp = FString::Printf(TEXT("%04d-%02d-%02d  %02d:%02d:%02d"),
            Now.GetYear(), Now.GetMonth(), Now.GetDay(), Now.GetHour(), Now.GetMinute(), Now.GetSecond());
    }
    const FString Stamp = LastStamp;     // frozen while frames are being dropped

    const float Right = Canvas->SizeX - 24.f;
    float W = 0.f, H = 0.f;
    GetTextSize(Stamp, W, H, Font, 1.2f);
    const float X = Right - W;
    const float Y = 22.f;

    const FLinearColor Text(0.95f, 0.95f, 0.92f, 0.85f);
    DrawText(Stamp, Text, X, Y, Font, 1.2f);
    DrawText(CameraUnitId, Text, X, Y + H + 2.f, Font, 1.2f);

    // REC with a blinking red dot - or, when the signal is breaking up, NO SIGNAL flickering in
    const double Time = GetWorld()->GetRealTimeSeconds();
    const bool bOn = FMath::Fmod(Time, 1.0) < 0.6;
    const bool bNoSignal = SignalLoss > 0.55f && FMath::FRand() < SignalLoss * 0.8f;
    const FString Rec = bNoSignal ? FString(TEXT("NO SIGNAL")) : FString(TEXT("REC"));
    float RW = 0.f, RH = 0.f;
    GetTextSize(Rec, RW, RH, Font, 1.2f);
    const float RecX = X - RW - 26.f;
    if (bOn && !bHeld && !bNoSignal)
    {
        DrawRect(FLinearColor(0.9f, 0.05f, 0.05f, 0.9f), RecX - 14.f, Y + RH * 0.5f - 4.f, 8.f, 8.f);
    }
    DrawText(Rec, bNoSignal ? FLinearColor(1.f, 0.2f, 0.15f, 0.95f) : Text, RecX, Y, Font, 1.2f);
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

void AMyHUD::DrawMenu(AAHPlayerController* MenuPC)
{
    UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;
    UFont* Medium = GEngine ? GEngine->GetMediumFont() : nullptr;
    UFont* Large = GEngine ? GEngine->GetLargeFont() : nullptr;
    const float W = Canvas->SizeX;
    const float H = Canvas->SizeY;
    const EAHMenu Which = MenuPC->GetMenu();

    DrawRect(FLinearColor(0.f, 0.f, 0.f, Which == EAHMenu::Title ? 0.72f : 0.6f), 0.f, 0.f, W, H);
    // scanlines, like paused footage on a review monitor
    for (float LineY = 0.f; LineY < H; LineY += 4.f)
    {
        DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.18f), 0.f, LineY, W, 1.f);
    }

    const float Left = W * 0.12f;
    float Y = H * 0.22f;
    const FLinearColor Amber(1.f, 0.66f, 0.2f, 0.95f);
    const FLinearColor Pale(0.92f, 0.92f, 0.88f, 0.9f);
    const FLinearColor Dim(0.6f, 0.6f, 0.58f, 0.75f);

    float TW = 0.f, TH = 0.f;
    if (Which == EAHMenu::Title)
    {
        DrawText(TEXT("R.S.U. EVIDENCE ARCHIVE  //  BODY-WORN CAMERA FOOTAGE"), Dim, Left, Y - 60.f, Small, 1.2f);
        DrawText(TEXT("SITE A-17"), Pale, Left, Y - 20.f, Large, 2.6f);
        GetTextSize(TEXT("SITE A-17"), TW, TH, Large, 2.6f);
        Y += TH - 10.f;
        DrawText(TEXT("UNIT AH-BC 0417  |  2ND CONTAINMENT TEAM  |  STATUS: NO CONTACT"), Amber, Left, Y, Small, 1.2f);
        Y += 70.f;
    }
    else
    {
        const TCHAR* Head = Which == EAHMenu::Pause ? TEXT("PLAYBACK PAUSED") : TEXT("SETTINGS");
        DrawText(Head, Pale, Left, Y - 20.f, Large, 2.f);
        GetTextSize(Head, TW, TH, Large, 2.f);
        Y += TH + 30.f;
        // the familiar pause bars
        if (Which == EAHMenu::Pause)
        {
            DrawRect(Amber, Left - 50.f, H * 0.22f - 12.f, 9.f, 34.f);
            DrawRect(Amber, Left - 36.f, H * 0.22f - 12.f, 9.f, 34.f);
        }
    }

    TArray<FAHMenuLine> Lines;
    MenuPC->GetMenuLines(Lines);
    TArray<FBox2D> Rects;
    const float RowW = FMath::Min(W * 0.5f, 720.f);
    const float RowH = 44.f;
    for (int32 i = 0; i < Lines.Num(); ++i)
    {
        const bool bSel = (i == MenuPC->GetSelected());
        const float RowY = Y + i * (RowH + 6.f);
        if (bSel)
        {
            DrawRect(FLinearColor(1.f, 0.66f, 0.2f, 0.16f), Left - 14.f, RowY - 6.f, RowW, RowH);
            DrawRect(Amber, Left - 14.f, RowY - 6.f, 3.f, RowH);
        }
        DrawText(Lines[i].Label, bSel ? Pale : Dim, Left, RowY, Medium, 1.f);
        if (!Lines[i].Value.IsEmpty())
        {
            const FString Val = FString::Printf(TEXT("<  %s  >"), *Lines[i].Value);
            float VW = 0.f, VH = 0.f;
            GetTextSize(Val, VW, VH, Medium, 1.f);
            DrawText(Val, bSel ? Amber : Dim, Left - 14.f + RowW - VW - 18.f, RowY, Medium, 1.f);
        }
        Rects.Add(FBox2D(FVector2D(Left - 14.f, RowY - 6.f), FVector2D(Left - 14.f + RowW, RowY - 6.f + RowH)));
    }
    MenuPC->SetMenuRects(Rects);

    const TCHAR* Hint = Which == EAHMenu::Settings
        ? TEXT("UP / DOWN select    LEFT / RIGHT or click a side to change    ESC back")
        : (Which == EAHMenu::Pause ? TEXT("UP / DOWN select    ENTER or click    ESC (P in the editor) resume")
                                   : TEXT("UP / DOWN select    ENTER or click"));
    DrawText(Hint, Dim, Left, H - 70.f, Small, 1.1f);
}
