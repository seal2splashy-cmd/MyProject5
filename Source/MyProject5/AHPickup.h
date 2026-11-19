// AHPickup.h
// Things to scavenge, picked up with F: a loaded 9 mm magazine, a bandage (heals slowly - you
// can't patch yourself up instantly), or a torch battery. Built from engine shapes so it works
// without imported assets. Docs/DOORS_PICKUPS.md

#pragma once

#include "CoreMinimal.h"
#include "AHInteractable.h"
#include "AHPickup.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class USceneComponent;

UENUM(BlueprintType)
enum class EAHPickupType : uint8
{
    Magazine,
    Bandage,
    Battery
};

UCLASS()
class MYPROJECT5_API AAHPickup : public AAHInteractable
{
    GENERATED_BODY()

public:
    AAHPickup();

    virtual void OnConstruction(const FTransform& Transform) override;

    virtual FString GetPrompt(const AMyFPSPawn* User) const override;
    virtual void Interact(AMyFPSPawn* User) override;

    EAHPickupType GetType() const { return Type; }

protected:
    UPROPERTY(EditAnywhere, Category = "Pickup")
    EAHPickupType Type = EAHPickupType::Magazine;

    /** Rounds in the magazine. */
    UPROPERTY(EditAnywhere, Category = "Pickup", meta = (EditCondition = "Type == EAHPickupType::Magazine", ClampMin = 1))
    int32 Rounds = 12;

    /** Health a bandage restores (over a few seconds). */
    UPROPERTY(EditAnywhere, Category = "Pickup", meta = (EditCondition = "Type == EAHPickupType::Bandage", ClampMin = 1))
    float HealAmount = 35.f;

    /** Fraction of a full torch battery. */
    UPROPERTY(EditAnywhere, Category = "Pickup", meta = (EditCondition = "Type == EAHPickupType::Battery", ClampMin = 0.05, ClampMax = 1))
    float BatteryAmount = 0.7f;

private:
    void BuildLook();

    UPROPERTY(VisibleAnywhere)
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Body;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* Detail;

    UPROPERTY()
    UStaticMesh* CubeMesh = nullptr;

    UPROPERTY()
    UStaticMesh* CylinderMesh = nullptr;
};
