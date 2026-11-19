#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FacilityInteractionPoint.h"
#include "FacilityMissionDirector.generated.h"

/**
 * Prototype playable mission, independent of the player's existing controls.
 * G = interact, while F torch and Q/E/Z/C lean bindings stay unchanged.
 * Uses the existing Gaunt C++ class if encounter spawning is enabled.
 */
UCLASS()
class MYPROJECT5_API AFacilityMissionDirector : public AActor
{
    GENERATED_BODY()

public:
    AFacilityMissionDirector();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bSpawnGauntAfterContainment = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    FVector GauntSpawnLocation = FVector(6650.f, -950.f, 110.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bShowPrototypeHUD = true;

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

private:
    bool bHasCard = false;
    bool bPower = false;
    bool bDoorOpen = false;
    bool bDecontaminated = false;
    bool bObservationRead = false;
    bool bContainmentInspected = false;
    bool bEscaped = false;
    bool bGauntSpawned = false;
    float UpdateTime = 0.f;

    void Interact();
    AFacilityInteractionPoint* FindTarget() const;
    FString Objective() const;
    FString Requirement(EFacilityAction Action) const;
    void Display(const FString& Text, const FColor& Color = FColor::Cyan) const;
    void SetPoweredLights(bool bOn);
    void OpenRestrictedVisualDoor();
    void SpawnExistingGaunt();
};
