#include "MyGameMode.h"
#include "MyFPSPawn.h"
#include "MyHUD.h"

AMyGameMode::AMyGameMode()
{
    DefaultPawnClass = AMyFPSPawn::StaticClass();
    HUDClass = AMyHUD::StaticClass();
}
