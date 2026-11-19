#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FacilityInteractionPoint.generated.h"

class UBoxComponent;
class UPointLightComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EFacilityAction : uint8
{
    AccessCard,
    RestorePower,
    RestrictedDoor,
    Decontamination,
    ObservationConsole,
    ContainmentConsole,
    Extraction
};

/**
 * Placement-only game interaction anchor: status light + optional locked-door
 * blocking volume. Uses no hand-models, scanned props or replacement art.
 * Meshy meshes remain independent scene actors.
 */
UCLASS()
class MYPROJECT5_API AFacilityInteractionPoint : public AActor
{
    GENERATED_BODY()

public:
    AFacilityInteractionPoint();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    EFacilityAction Action = EFacilityAction::AccessCard;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    FString Label = TEXT("Facility control");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    float ReachDistance = 240.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
    UBoxComponent* DoorBlocker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
    UPointLightComponent* Indicator;

    UFUNCTION(BlueprintCallable, Category = "Facility")
    void SetCompleted(bool bInCompleted);

    UFUNCTION(BlueprintCallable, Category = "Facility")
    void SetDoorOpen(bool bOpen);

    bool IsCompleted() const { return bCompleted; }

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    USceneComponent* SceneRoot;

    bool bCompleted = false;
};
