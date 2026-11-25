// AHPlayerController.h
// Menus (drawn by AMyHUD in the bodycam-archive style, no UMG/Blueprints) + settings.
//
//  Title   once per session when the game starts: CONTINUE (if there's a checkpoint) / NEW GAME /
//          SETTINGS / QUIT. The game is paused behind it.
//  Pause   Esc (or P - Esc stops Play-In-Editor): RESUME / SETTINGS / RELOAD CHECKPOINT /
//          QUIT TO TITLE / QUIT GAME.
//  Settings  mouse sensitivity, invert Y, field of view, master volume, brightness, bodycam effects
//          (full / reduced / off), motion blur. Saved at once (UAHSettingsSave), applied live.
//
// Mouse: hover + click (click the left/right half of a setting to change it). Keyboard: Up/Down or
// W/S, Left/Right or A/D, Enter/Space, Esc/P = back. `ah.SkipTitle 1` skips the title screen.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AHPlayerController.generated.h"

class UAHSettingsSave;

UENUM()
enum class EAHMenu : uint8
{
    None,
    Title,
    Pause,
    Settings
};

/** One line of the open menu, for the HUD. */
struct FAHMenuLine
{
    FString Label;
    FString Value;          // empty for plain buttons
};

UCLASS()
class MYPROJECT5_API AAHPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AAHPlayerController();

    virtual void PlayerTick(float DeltaTime) override;

    EAHMenu GetMenu() const { return Menu; }
    int32 GetSelected() const { return Selected; }
    void GetMenuLines(TArray<FAHMenuLine>& Out) const;
    /** The HUD tells us where it drew each line (screen pixels), for the mouse. */
    void SetMenuRects(const TArray<FBox2D>& Rects) { MenuRects = Rects; }
    bool HasCheckpoint() const;

    void ApplySettings();

protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;

private:
    void OpenMenu(EAHMenu NewMenu);
    void CloseMenu();
    void Activate(int32 Index);
    void Adjust(int32 Index, int32 Direction);
    int32 LineCount() const;
    void TitleStarted();

    UPROPERTY(Transient)
    TObjectPtr<UAHSettingsSave> Settings;

    EAHMenu Menu = EAHMenu::None;
    EAHMenu SettingsReturnTo = EAHMenu::Pause;
    int32 Selected = 0;
    TArray<FBox2D> MenuRects;
    FVector2D LastMouse = FVector2D::ZeroVector;
    bool bTitlePending = false;
};
