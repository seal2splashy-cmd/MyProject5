// AHPlayerController.cpp

#include "AHPlayerController.h"
#include "AHSettingsSave.h"
#include "AHMissionProgress.h"
#include "MyFPSPawn.h"
#include "BodycamLensComponent.h"
#include "AudioDevice.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
    TAutoConsoleVariable<int32> CVarSkipTitle(TEXT("ah.SkipTitle"), 0,
        TEXT("1 = start straight into the game without the title screen."));

    enum class EAct : uint8
    {
        Continue, NewGame, Settings, Quit,                      // title
        Resume, Reload, QuitToTitle,                            // pause (+ Settings, Quit)
        Sensitivity, InvertY, FOV, Volume, Brightness, Effects, MotionBlur, Back   // settings
    };

    void BuildMenu(EAHMenu Menu, bool bCheckpoint, TArray<EAct>& Out)
    {
        Out.Reset();
        switch (Menu)
        {
        case EAHMenu::Title:
            if (bCheckpoint)
            {
                Out.Add(EAct::Continue);
            }
            Out.Append({ EAct::NewGame, EAct::Settings, EAct::Quit });
            break;
        case EAHMenu::Pause:
            Out.Append({ EAct::Resume, EAct::Settings, EAct::Reload, EAct::QuitToTitle, EAct::Quit });
            break;
        case EAHMenu::Settings:
            Out.Append({ EAct::Sensitivity, EAct::InvertY, EAct::FOV, EAct::Volume, EAct::Brightness,
                         EAct::Effects, EAct::MotionBlur, EAct::Back });
            break;
        default:
            break;
        }
    }

    UAHMissionProgress* Progress(const UObject* Ctx)
    {
        const UGameInstance* GI = UGameplayStatics::GetGameInstance(Ctx);
        return GI ? GI->GetSubsystem<UAHMissionProgress>() : nullptr;
    }

    bool Pressed(const APlayerController* PC, std::initializer_list<FKey> Keys)
    {
        for (const FKey& K : Keys)
        {
            if (PC->WasInputKeyJustPressed(K))
            {
                return true;
            }
        }
        return false;
    }
}

AAHPlayerController::AAHPlayerController()
{
    PrimaryActorTick.bTickEvenWhenPaused = true;    // menus keep working while the game is paused
    bShouldPerformFullTickWhenPaused = true;
}

void AAHPlayerController::BeginPlay()
{
    Super::BeginPlay();
    Settings = UAHSettingsSave::LoadOrCreate();
    ApplySettings();

    if (IsLocalController())
    {
        UAHMissionProgress* P = Progress(this);
        bTitlePending = P && !P->IsTitleShown() && CVarSkipTitle.GetValueOnGameThread() == 0;
        if (P && !bTitlePending)
        {
            P->SetTitleShown(true);
        }
    }
}

void AAHPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ApplySettings();
}

bool AAHPlayerController::HasCheckpoint() const
{
    const UAHMissionProgress* P = Progress(this);
    int32 Stage = 0;
    FVector Where;
    float Yaw = 0.f;
    float Elapsed = 0.f;
    return P && P->GetCheckpoint(UGameplayStatics::GetCurrentLevelName(this), Stage, Where, Yaw, Elapsed) && Stage > 0;
}

void AAHPlayerController::ApplySettings()
{
    if (!Settings)
    {
        return;
    }
    if (AMyFPSPawn* P = Cast<AMyFPSPawn>(GetPawn()))
    {
        P->SetLookSensitivity(Settings->MouseSensitivity);
        P->SetInvertY(Settings->bInvertY);
        P->SetBaseFOV(Settings->FieldOfView);
        if (UBodycamLensComponent* Lens = P->GetBodycamLens())
        {
            Lens->SetEffectsScale(Settings->BodycamEffects >= 2 ? 1.f : (Settings->BodycamEffects == 1 ? 0.45f : 0.f));
            Lens->SetMotionBlurEnabled(Settings->bMotionBlur);
            Lens->SetBrightness(Settings->Brightness);
        }
    }
    if (UWorld* W = GetWorld())
    {
        if (FAudioDeviceHandle Audio = W->GetAudioDevice())
        {
            Audio->SetTransientPrimaryVolume(FMath::Clamp(Settings->MasterVolume, 0.f, 1.f));
        }
    }
}

// ------------------------------------------------------------------ menu state

void AAHPlayerController::OpenMenu(EAHMenu NewMenu)
{
    if (NewMenu == EAHMenu::Settings)
    {
        SettingsReturnTo = Menu == EAHMenu::None ? EAHMenu::Pause : Menu;
    }
    Menu = NewMenu;
    Selected = 0;
    MenuRects.Reset();
    UGameplayStatics::SetGamePaused(this, true);
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
}

void AAHPlayerController::CloseMenu()
{
    Menu = EAHMenu::None;
    MenuRects.Reset();
    UGameplayStatics::SetGamePaused(this, false);
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}

void AAHPlayerController::TitleStarted()
{
    if (UAHMissionProgress* P = Progress(this))
    {
        P->SetTitleShown(true);
    }
}

int32 AAHPlayerController::LineCount() const
{
    TArray<EAct> Acts;
    BuildMenu(Menu, HasCheckpoint(), Acts);
    return Acts.Num();
}

void AAHPlayerController::GetMenuLines(TArray<FAHMenuLine>& Out) const
{
    Out.Reset();
    TArray<EAct> Acts;
    BuildMenu(Menu, HasCheckpoint(), Acts);
    const UAHSettingsSave* S = Settings;
    for (EAct A : Acts)
    {
        FAHMenuLine L;
        switch (A)
        {
        case EAct::Continue:    L.Label = TEXT("CONTINUE"); break;
        case EAct::NewGame:     L.Label = TEXT("NEW GAME"); break;
        case EAct::Settings:    L.Label = TEXT("SETTINGS"); break;
        case EAct::Quit:        L.Label = TEXT("QUIT GAME"); break;
        case EAct::Resume:      L.Label = TEXT("RESUME"); break;
        case EAct::Reload:      L.Label = TEXT("RELOAD LAST CHECKPOINT"); break;
        case EAct::QuitToTitle: L.Label = TEXT("QUIT TO TITLE"); break;
        case EAct::Back:        L.Label = TEXT("BACK"); break;
        case EAct::Sensitivity:
            L.Label = TEXT("MOUSE SENSITIVITY");
            L.Value = S ? FString::Printf(TEXT("%.1f"), S->MouseSensitivity) : TEXT("-");
            break;
        case EAct::InvertY:
            L.Label = TEXT("INVERT MOUSE Y");
            L.Value = (S && S->bInvertY) ? TEXT("ON") : TEXT("OFF");
            break;
        case EAct::FOV:
            L.Label = TEXT("FIELD OF VIEW");
            L.Value = S ? FString::Printf(TEXT("%d"), FMath::RoundToInt(S->FieldOfView)) : TEXT("-");
            break;
        case EAct::Volume:
            L.Label = TEXT("MASTER VOLUME");
            L.Value = S ? FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S->MasterVolume * 100.f)) : TEXT("-");
            break;
        case EAct::Brightness:
            L.Label = TEXT("BRIGHTNESS");
            L.Value = S ? FString::Printf(TEXT("%+.1f"), S->Brightness) : TEXT("-");
            break;
        case EAct::Effects:
            L.Label = TEXT("BODYCAM EFFECTS");
            L.Value = !S ? TEXT("-") : (S->BodycamEffects >= 2 ? TEXT("FULL") : (S->BodycamEffects == 1 ? TEXT("REDUCED") : TEXT("OFF")));
            break;
        case EAct::MotionBlur:
            L.Label = TEXT("MOTION BLUR");
            L.Value = (S && S->bMotionBlur) ? TEXT("ON") : TEXT("OFF");
            break;
        }
        Out.Add(L);
    }
}

void AAHPlayerController::Adjust(int32 Index, int32 Direction)
{
    TArray<EAct> Acts;
    BuildMenu(Menu, HasCheckpoint(), Acts);
    if (!Settings || !Acts.IsValidIndex(Index))
    {
        return;
    }
    UAHSettingsSave* S = Settings;
    const float D = static_cast<float>(Direction);
    switch (Acts[Index])
    {
    case EAct::Sensitivity: S->MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((S->MouseSensitivity + 0.1f * D) * 10.f) / 10.f, 0.2f, 3.f); break;
    case EAct::InvertY:     S->bInvertY = !S->bInvertY; break;
    case EAct::FOV:         S->FieldOfView = FMath::Clamp(S->FieldOfView + 5.f * D, 80.f, 115.f); break;
    case EAct::Volume:      S->MasterVolume = FMath::Clamp(FMath::RoundToFloat((S->MasterVolume + 0.1f * D) * 10.f) / 10.f, 0.f, 1.f); break;
    case EAct::Brightness:  S->Brightness = FMath::Clamp(S->Brightness + 0.5f * D, -2.f, 2.f); break;
    case EAct::Effects:     S->BodycamEffects = (S->BodycamEffects + (Direction > 0 ? 1 : 2)) % 3; break;
    case EAct::MotionBlur:  S->bMotionBlur = !S->bMotionBlur; break;
    default: return;        // buttons don't adjust
    }
    S->Save();
    ApplySettings();
}

void AAHPlayerController::Activate(int32 Index)
{
    TArray<EAct> Acts;
    BuildMenu(Menu, HasCheckpoint(), Acts);
    if (!Acts.IsValidIndex(Index))
    {
        return;
    }
    switch (Acts[Index])
    {
    case EAct::Continue:
    case EAct::Resume:
        TitleStarted();
        CloseMenu();
        break;
    case EAct::NewGame:
    {
        TitleStarted();
        const bool bFresh = !HasCheckpoint();
        if (UAHMissionProgress* P = Progress(this))
        {
            P->Clear();
        }
        CloseMenu();
        if (!bFresh)
        {
            UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
        }
        break;
    }
    case EAct::Settings:
        OpenMenu(EAHMenu::Settings);
        break;
    case EAct::Reload:
        CloseMenu();
        UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
        break;
    case EAct::QuitToTitle:
        if (UAHMissionProgress* P = Progress(this))
        {
            P->SetTitleShown(false);
        }
        CloseMenu();
        UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
        break;
    case EAct::Quit:
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
        break;
    case EAct::Back:
        OpenMenu(SettingsReturnTo);
        break;
    default:
        Adjust(Index, 1);   // Enter / click on a setting steps it forward
        break;
    }
}

// ------------------------------------------------------------------ input (polled, works while paused)

void AAHPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!IsLocalController())
    {
        return;
    }
    if (bTitlePending && GetPawn())
    {
        bTitlePending = false;
        OpenMenu(EAHMenu::Title);
        return;
    }

    const bool bBack = Pressed(this, { EKeys::Escape, EKeys::P });
    if (Menu == EAHMenu::None)
    {
        const AMyFPSPawn* P = Cast<AMyFPSPawn>(GetPawn());
        if (bBack && !(P && P->IsDead()))
        {
            OpenMenu(EAHMenu::Pause);
        }
        return;
    }

    if (bBack)
    {
        if (Menu == EAHMenu::Pause)
        {
            CloseMenu();
        }
        else if (Menu == EAHMenu::Settings)
        {
            OpenMenu(SettingsReturnTo);
        }
        return;
    }

    const int32 Count = LineCount();
    if (Count <= 0)
    {
        return;
    }
    if (Pressed(this, { EKeys::Up, EKeys::W }))
    {
        Selected = (Selected + Count - 1) % Count;
    }
    if (Pressed(this, { EKeys::Down, EKeys::S }))
    {
        Selected = (Selected + 1) % Count;
    }
    if (Pressed(this, { EKeys::Left, EKeys::A }))
    {
        Adjust(Selected, -1);
    }
    if (Pressed(this, { EKeys::Right, EKeys::D }))
    {
        Adjust(Selected, 1);
    }
    if (Pressed(this, { EKeys::Enter, EKeys::SpaceBar }))
    {
        Activate(Selected);
        return;
    }

    // Mouse: hover selects (only when it moves, so it doesn't fight the keyboard), click acts
    float MX = 0.f, MY = 0.f;
    if (GetMousePosition(MX, MY))
    {
        const FVector2D Mouse(MX, MY);
        const bool bMoved = !Mouse.Equals(LastMouse, 0.5);
        LastMouse = Mouse;
        for (int32 i = 0; i < MenuRects.Num() && i < Count; ++i)
        {
            if (!MenuRects[i].IsInside(Mouse))
            {
                continue;
            }
            if (bMoved)
            {
                Selected = i;
            }
            if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
            {
                Selected = i;
                TArray<FAHMenuLine> Lines;
                GetMenuLines(Lines);
                if (Lines.IsValidIndex(i) && !Lines[i].Value.IsEmpty())
                {
                    Adjust(i, Mouse.X < MenuRects[i].GetCenter().X ? -1 : 1);   // left half down, right half up
                }
                else
                {
                    Activate(i);
                }
            }
            break;
        }
    }
}
