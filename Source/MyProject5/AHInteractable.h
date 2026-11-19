// AHInteractable.h
// Anything the player can use with F while looking at it (doors, pickups).
// AMyFPSPawn traces from the camera each frame; the HUD shows "[F] <prompt>".

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHInteractable.generated.h"

class AMyFPSPawn;

UCLASS(Abstract)
class MYPROJECT5_API AAHInteractable : public AActor
{
    GENERATED_BODY()

public:
    /** Text for the HUD, e.g. "Open door". Shown even when it can't be used ("Locked", "Pouch full"). */
    virtual FString GetPrompt(const AMyFPSPawn* User) const { return FString(); }

    /** The player pressed F while looking at it. */
    virtual void Interact(AMyFPSPawn* User) {}

    /** How close the camera has to be (cm). */
    float GetInteractRange() const { return InteractRange; }

protected:
    UPROPERTY(EditAnywhere, Category = "Interaction", meta = (ClampMin = 50))
    float InteractRange = 210.f;
};
