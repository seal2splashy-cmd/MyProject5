// FacilityInteractionPoint.h
// One step of the A-07 facility mission (card, power, security door, decon, consoles, extraction).
// It's an AAHInteractable, so it works with the player's normal F key and "[F] ..." HUD prompt
// (G still works too, through AFacilityMissionDirector). The director decides what using it does.
//
// Looks: a small device built from engine shapes (lockbox + keycard, breaker cabinet, card reader,
// decon panel, terminal, radio beacon) on a post down to the floor, with a status light:
//   red = not yet / locked, amber pulsing = the current objective, green = done.
// Turn bShowPanel off when a Meshy model already sits there: an invisible box still catches F.

#pragma once

#include "CoreMinimal.h"
#include "AHInteractable.h"
#include "FacilityInteractionPoint.generated.h"

class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class AFacilityMissionDirector;

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

UENUM(BlueprintType)
enum class EFacilityPointState : uint8
{
    Locked,     // an earlier step isn't done
    Current,    // the step the player should do now
    Done
};

UCLASS()
class MYPROJECT5_API AFacilityInteractionPoint : public AAHInteractable
{
    GENERATED_BODY()

public:
    AFacilityInteractionPoint();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;

    // AAHInteractable
    virtual FString GetPrompt(const AMyFPSPawn* User) const override;
    virtual void Interact(AMyFPSPawn* User) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    EFacilityAction Action = EFacilityAction::AccessCard;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    FString Label = TEXT("Facility control");

    /** How far away G finds it (the F trace uses this + a little, measured to the device). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility")
    float ReachDistance = 240.f;

    /** Build the little device. Off = invisible (a Meshy console is already there), F still works. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility|Look")
    bool bShowPanel = true;

    /** Where the device sits, relative to the actor. Zero = automatic (the security door's
     *  card reader goes beside the doorway, everything else on the actor). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility|Look")
    FVector PanelOffset = FVector::ZeroVector;

    /** Which way the device's front (screen) faces, degrees, added to the actor's yaw. 0 = +X. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility|Look")
    float PanelYaw = 0.f;

    /** A post from the device down to the floor (found with a trace). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facility|Look")
    bool bPedestal = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
    UBoxComponent* DoorBlocker;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility")
    UPointLightComponent* Indicator;

    UFUNCTION(BlueprintCallable, Category = "Facility")
    void SetCompleted(bool bInCompleted);

    UFUNCTION(BlueprintCallable, Category = "Facility")
    void SetDoorOpen(bool bOpen);

    void SetPointState(EFacilityPointState NewState);
    EFacilityPointState GetPointState() const { return PointState; }
    bool IsCompleted() const { return PointState == EFacilityPointState::Done; }

    /** Where the device actually is (for sounds / distance checks). */
    FVector GetDeviceLocation() const;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Look")
    USceneComponent* PanelRoot;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Look")
    UStaticMeshComponent* Body;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Look")
    UStaticMeshComponent* Screen;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Look")
    UStaticMeshComponent* Detail;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Look")
    UStaticMeshComponent* Pedestal;

    /** Invisible box the F trace hits (Visibility only: you don't bump into it). */
    UPROPERTY(VisibleAnywhere, Category = "Facility")
    UBoxComponent* UseBox;

    UPROPERTY()
    UStaticMesh* CubeMesh;

    UPROPERTY()
    UStaticMesh* CylinderMesh;

    EFacilityPointState PointState = EFacilityPointState::Locked;
    float PulseTime = 0.f;
    TWeakObjectPtr<AFacilityMissionDirector> CachedDirector;

    AFacilityMissionDirector* FindDirector() const;
    void BuildLook();
    void ApplyStateLook();
    void SetPartColour(UStaticMeshComponent* Part, const FLinearColor& Colour);
};
