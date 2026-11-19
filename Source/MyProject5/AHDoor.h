// AHDoor.h
// A hinged door (single or double) built from engine shapes, so it works in any level with no
// imported assets. F opens it away from you / closes it; sprinting into it slams it open.
// Enemies open it when they walk into it (a hunting one slams it). Opening, closing and slamming
// are noises enemies hear. A locked door rattles and blocks the NavMesh so enemies path round it.
// Docs/DOORS_PICKUPS.md

#pragma once

#include "CoreMinimal.h"
#include "AHInteractable.h"
#include "AHDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class USceneComponent;

UCLASS()
class MYPROJECT5_API AAHDoor : public AAHInteractable
{
    GENERATED_BODY()

public:
    AAHDoor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaTime) override;

    virtual FString GetPrompt(const AMyFPSPawn* User) const override;
    virtual void Interact(AMyFPSPawn* User) override;

    /** Swing open away from From (so it never swings into whoever opened it). Slam = fast and loud. */
    void OpenAwayFrom(const FVector& From, bool bSlam, AActor* Opener);
    void Close(AActor* Closer);

    bool IsOpen() const { return TargetAlpha > 0.5f; }
    bool IsLocked() const { return bLocked; }
    void SetLocked(bool bInLocked);

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // ---- Shape (cm). The actor's origin is the middle of the doorway at floor level; the door
    //      spans local Y (width), its faces look along local X.
    UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = 40))
    float Width = 236.f;

    UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = 100))
    float Height = 230.f;

    UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = 2))
    float Thickness = 5.f;

    /** Two leaves meeting in the middle (wide doorways) instead of one. */
    UPROPERTY(EditAnywhere, Category = "Door")
    bool bDoubleDoor = true;

    /** Draw a frame (jambs + head) around the leaves. Off if the wall opening already looks framed. */
    UPROPERTY(EditAnywhere, Category = "Door")
    bool bShowFrame = true;

    UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = 30, ClampMax = 170))
    float OpenAngle = 100.f;

    /** Seconds to swing fully open/closed normally; a slam is 3x faster. */
    UPROPERTY(EditAnywhere, Category = "Door", meta = (ClampMin = 0.1))
    float SwingTime = 0.9f;

    UPROPERTY(EditAnywhere, Category = "Door")
    bool bStartOpen = false;

    UPROPERTY(EditAnywhere, Category = "Door")
    bool bLocked = false;

    UPROPERTY(EditAnywhere, Category = "Door")
    FString LockedPrompt = TEXT("Locked");

    /** Enemies walking into it open it. */
    UPROPERTY(EditAnywhere, Category = "Door")
    bool bEnemiesCanOpen = true;

    UPROPERTY(EditAnywhere, Category = "Door|Look")
    TSoftObjectPtr<UMaterialInterface> LeafMaterial;

    UPROPERTY(EditAnywhere, Category = "Door|Look")
    TSoftObjectPtr<UMaterialInterface> FrameMaterial;

    // ---- Noise (cm enemies hear it from)
    UPROPERTY(EditAnywhere, Category = "Door|Noise")
    float OpenNoiseRange = 450.f;

    UPROPERTY(EditAnywhere, Category = "Door|Noise")
    float CloseNoiseRange = 650.f;

    UPROPERTY(EditAnywhere, Category = "Door|Noise")
    float SlamNoiseRange = 1600.f;

    UPROPERTY(EditAnywhere, Category = "Door|Noise")
    float RattleNoiseRange = 350.f;

private:
    void Layout();
    void ApplySwing();
    void DoorSound(const TCHAR* Prefix, int32 Variants, float Volume, float NoiseRange, AActor* NoiseMaker);

    UPROPERTY(VisibleAnywhere)
    USceneComponent* Root;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* HingeLeft;

    UPROPERTY(VisibleAnywhere)
    USceneComponent* HingeRight;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* LeafLeft;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* LeafRight;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* HandleLeft;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* HandleRight;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* JambLeft;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* JambRight;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* FrameHead;

    /** Enemies entering this open the door. */
    UPROPERTY(VisibleAnywhere)
    UBoxComponent* EnemyTrigger;

    float OpenAlpha = 0.f;     // 0 shut .. 1 open
    float TargetAlpha = 0.f;
    float SwingSign = 1.f;     // +1 swings towards local -X, -1 towards +X
    float SwingSpeed = 1.f;    // alpha per second
    float LastRattleTime = -10.f;
};
