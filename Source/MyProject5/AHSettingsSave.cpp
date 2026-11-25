// AHSettingsSave.cpp

#include "AHSettingsSave.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    const TCHAR* SlotName = TEXT("AHSettings");
}

UAHSettingsSave* UAHSettingsSave::LoadOrCreate()
{
    if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        if (UAHSettingsSave* Loaded = Cast<UAHSettingsSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
        {
            return Loaded;
        }
    }
    return Cast<UAHSettingsSave>(UGameplayStatics::CreateSaveGameObject(UAHSettingsSave::StaticClass()));
}

void UAHSettingsSave::Save()
{
    UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}
