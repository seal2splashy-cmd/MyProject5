#include "FacilityInteractionPoint.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"

AFacilityInteractionPoint::AFacilityInteractionPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    Indicator = CreateDefaultSubobject<UPointLightComponent>(TEXT("Indicator"));
    Indicator->SetupAttachment(RootComponent);
    Indicator->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
    Indicator->SetIntensity(140.f);
    Indicator->SetAttenuationRadius(120.f);
    Indicator->SetLightColor(FLinearColor(1.f, 0.18f, 0.11f));

    DoorBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlocker"));
    DoorBlocker->SetupAttachment(RootComponent);
    DoorBlocker->SetBoxExtent(FVector(25.f, 135.f, 130.f));
    DoorBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DoorBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    DoorBlocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}

void AFacilityInteractionPoint::BeginPlay()
{
    Super::BeginPlay();
    // Only the restricted security door acts as a physical barrier.
    if (Action == EFacilityAction::RestrictedDoor)
    {
        SetDoorOpen(false);
    }
}

void AFacilityInteractionPoint::SetCompleted(bool bInCompleted)
{
    bCompleted = bInCompleted;
    if (Indicator)
    {
        Indicator->SetLightColor(bCompleted ?
            FLinearColor(0.12f, 0.85f, 0.27f) :
            FLinearColor(1.f, 0.18f, 0.11f));
        Indicator->SetIntensity(bCompleted ? 180.f : 140.f);
    }
}

void AFacilityInteractionPoint::SetDoorOpen(bool bOpen)
{
    if (Action != EFacilityAction::RestrictedDoor || !DoorBlocker)
    {
        return;
    }
    DoorBlocker->SetCollisionEnabled(bOpen ?
        ECollisionEnabled::NoCollision :
        ECollisionEnabled::QueryAndPhysics);
    DoorBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    DoorBlocker->SetCollisionResponseToChannel(ECC_Pawn, bOpen ? ECR_Ignore : ECR_Block);
    if (bOpen)
    {
        SetCompleted(true);
    }
}
