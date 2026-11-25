#include "MyGameMode.h"
#include "MyFPSPawn.h"
#include "MyHUD.h"
#include "AHPlayerController.h"

AMyGameMode::AMyGameMode()
{
    DefaultPawnClass = AMyFPSPawn::StaticClass();
    HUDClass = AMyHUD::StaticClass();
    PlayerControllerClass = AAHPlayerController::StaticClass();   // pause / title / settings
}
