// FacilityInteractionPoint.cpp

#include "FacilityInteractionPoint.h"
#include "FacilityMissionDirector.h"
#include "MyFPSPawn.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // What each device is made of. Sizes in cm, the front (screen) faces +X, body centred on the panel root.
    struct FDeviceLook
    {
        FVector BodySize;
        FLinearColor BodyColour;
        FVector ScreenSize;
        FVector ScreenCentre;
        FVector DetailSize;
        FVector DetailCentre;
        FRotator DetailRotation;
        bool bDetailCylinder;
        FLinearColor DetailColour;
    };

    FDeviceLook LookFor(EFacilityAction Which)
    {
        const FLinearColor Dark(0.035f, 0.04f, 0.045f);
        const FLinearColor Steel(0.16f, 0.18f, 0.19f);
        switch (Which)
        {
        case EFacilityAction::AccessCard:
            // A steel lockbox with the maintenance keycard lying on the lid
            return { FVector(24.f, 34.f, 12.f), Dark, FVector(0.6f, 8.f, 2.f), FVector(12.3f, 0.f, 3.f),
                     FVector(5.4f, 8.6f, 0.4f), FVector(0.f, 0.f, 6.2f), FRotator(0.f, 18.f, 0.f), false,
                     FLinearColor(0.85f, 0.72f, 0.18f) };
        case EFacilityAction::RestorePower:
            // Breaker cabinet with a status strip and a big red lever (down = off)
            return { FVector(22.f, 46.f, 64.f), Steel, FVector(0.6f, 30.f, 6.f), FVector(11.3f, 0.f, 22.f),
                     FVector(4.f, 4.f, 20.f), FVector(13.f, 14.f, -10.f), FRotator::ZeroRotator, false,
                     FLinearColor(0.6f, 0.04f, 0.03f) };
        case EFacilityAction::RestrictedDoor:
            // Card reader: small black box, screen, card slot
            return { FVector(8.f, 14.f, 22.f), Dark, FVector(0.6f, 10.f, 7.f), FVector(4.3f, 0.f, 5.f),
                     FVector(1.f, 10.f, 1.5f), FVector(4.5f, 0.f, -5.f), FRotator::ZeroRotator, false,
                     FLinearColor(0.5f, 0.52f, 0.55f) };
        case EFacilityAction::Decontamination:
            // Yellow decon control panel with a big red mushroom button
            return { FVector(14.f, 40.f, 34.f), FLinearColor(0.55f, 0.38f, 0.06f), FVector(0.6f, 26.f, 14.f),
                     FVector(7.3f, 0.f, 6.f), FVector(8.f, 8.f, 4.f), FVector(8.f, 0.f, -9.f), FRotator(90.f, 0.f, 0.f),
                     true, FLinearColor(0.6f, 0.03f, 0.02f) };
        case EFacilityAction::Extraction:
            // Olive field radio beacon with a whip antenna
            return { FVector(30.f, 30.f, 40.f), FLinearColor(0.11f, 0.13f, 0.065f), FVector(0.6f, 18.f, 10.f),
                     FVector(15.3f, 0.f, 8.f), FVector(1.5f, 1.5f, 90.f), FVector(-8.f, 8.f, 65.f), FRotator::ZeroRotator,
                     true, Dark };
        case EFacilityAction::ObservationConsole:
        case EFacilityAction::ContainmentConsole:
        default:
            // Monitor on an arm with a keyboard in front
            return { FVector(14.f, 62.f, 42.f), Dark, FVector(0.6f, 56.f, 36.f), FVector(7.3f, 0.f, 0.f),
                     FVector(24.f, 50.f, 3.f), FVector(14.f, 0.f, -22.f), FRotator::ZeroRotator, false, Dark };
        }
    }

    void SetupPart(UStaticMeshComponent* Part, bool bSolid)
    {
        if (bSolid)
        {
            // You bump into the device, the F trace hits it, the NavMesh ignores it (it's small)
            Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Part->SetCollisionResponseToAllChannels(ECR_Block);
        }
        else
        {
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        Part->SetCanEverAffectNavigation(false);
        Part->SetGenerateOverlapEvents(false);
    }
}

AFacilityInteractionPoint::AFacilityInteractionPoint()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;   // only ticks while it's the current objective (pulse)

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
    CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    PanelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PanelRoot"));
    PanelRoot->SetupAttachment(RootComponent);

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(PanelRoot);
    Screen = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Screen"));
    Screen->SetupAttachment(PanelRoot);
    Detail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Detail"));
    Detail->SetupAttachment(PanelRoot);
    Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
    Pedestal->SetupAttachment(PanelRoot);
    SetupPart(Body, true);
    SetupPart(Pedestal, true);
    SetupPart(Screen, false);
    SetupPart(Detail, false);

    UseBox = CreateDefaultSubobject<UBoxComponent>(TEXT("UseBox"));
    UseBox->SetupAttachment(PanelRoot);
    UseBox->SetBoxExtent(FVector(30.f, 40.f, 40.f));
    UseBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    UseBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    UseBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    UseBox->SetCanEverAffectNavigation(false);
    UseBox->SetGenerateOverlapEvents(false);

    Indicator = CreateDefaultSubobject<UPointLightComponent>(TEXT("Indicator"));
    Indicator->SetupAttachment(PanelRoot);
    Indicator->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
    Indicator->SetIntensity(60.f);
    Indicator->SetAttenuationRadius(140.f);
    Indicator->SetLightColor(FLinearColor(1.f, 0.18f, 0.11f));
    Indicator->SetCastShadows(false);

    DoorBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorBlocker"));
    DoorBlocker->SetupAttachment(RootComponent);
    DoorBlocker->SetBoxExtent(FVector(25.f, 135.f, 130.f));
    DoorBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DoorBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    DoorBlocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

    InteractRange = ReachDistance + 40.f;
}

void AFacilityInteractionPoint::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildLook();
}

void AFacilityInteractionPoint::BeginPlay()
{
    Super::BeginPlay();
    InteractRange = ReachDistance + 40.f;
    BuildLook();

    // Only the restricted security door acts as a physical barrier.
    if (Action == EFacilityAction::RestrictedDoor)
    {
        SetDoorOpen(false);
    }
    ApplyStateLook();
}

void AFacilityInteractionPoint::BuildLook()
{
    // Where the device goes. The security door's reader sits beside the doorway, facing back
    // down the corridor at you, unless you've placed it yourself.
    const bool bAutoPlace = PanelOffset.IsNearlyZero();
    FVector Offset = PanelOffset;
    float Yaw = PanelYaw;
    if (bAutoPlace && Action == EFacilityAction::RestrictedDoor)
    {
        Offset = FVector(-80.f, -150.f, 0.f);
        Yaw += 180.f;
    }
    PanelRoot->SetRelativeLocation(Offset);
    PanelRoot->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));

    const FDeviceLook Look = LookFor(Action);

    auto Shape = [this](UStaticMeshComponent* Part, UStaticMesh* ShapeMesh, const FVector& SizeCm,
        const FVector& Centre, const FRotator& Rot, const FLinearColor& Colour)
    {
        if (ShapeMesh)
        {
            Part->SetStaticMesh(ShapeMesh);
        }
        // Engine basic shapes are 100 cm across (the cylinder's axis is Z)
        Part->SetRelativeScale3D(SizeCm / 100.f);
        Part->SetRelativeLocation(Centre);
        Part->SetRelativeRotation(Rot);
        SetPartColour(Part, Colour);
    };

    Shape(Body, CubeMesh, Look.BodySize, FVector::ZeroVector, FRotator::ZeroRotator, Look.BodyColour);
    Shape(Screen, CubeMesh, Look.ScreenSize, Look.ScreenCentre, FRotator::ZeroRotator, FLinearColor(0.3f, 0.02f, 0.02f));
    Shape(Detail, Look.bDetailCylinder ? CylinderMesh : CubeMesh, Look.DetailSize, Look.DetailCentre,
        Look.DetailRotation, Look.DetailColour);

    // A post down to whatever is under the device (floor, crate, desk)
    float PostHeight = 0.f;
    if (bShowPanel && bPedestal)
    {
        if (UWorld* W = GetWorld())
        {
            const FVector Bottom = PanelRoot->GetComponentLocation() - FVector(0.f, 0.f, Look.BodySize.Z * 0.5f);
            FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityPedestal), false, this);
            FHitResult Hit;
            if (W->LineTraceSingleByChannel(Hit, Bottom, Bottom - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
            {
                PostHeight = Hit.Distance;
            }
        }
    }
    const FVector PostSize(8.f, 8.f, FMath::Max(PostHeight, 1.f));
    Shape(Pedestal, CubeMesh, PostSize, FVector(0.f, 0.f, -Look.BodySize.Z * 0.5f - PostHeight * 0.5f),
        FRotator::ZeroRotator, FLinearColor(0.03f, 0.03f, 0.035f));
    Pedestal->SetVisibility(bShowPanel && PostHeight > 2.f);
    Pedestal->SetCollisionEnabled(bShowPanel && PostHeight > 2.f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

    for (UStaticMeshComponent* Part : { Body, Screen, Detail })
    {
        Part->SetVisibility(bShowPanel);
    }
    Body->SetCollisionEnabled(bShowPanel ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

    // The F target: a bit bigger than the device so it isn't fiddly
    UseBox->SetBoxExtent(bShowPanel ? Look.BodySize * 0.5f + FVector(6.f) : FVector(30.f, 40.f, 40.f));
    UseBox->SetRelativeLocation(FVector::ZeroVector);

    Indicator->SetRelativeLocation(bShowPanel
        ? FVector(Look.BodySize.X * 0.5f + 6.f, 0.f, Look.BodySize.Z * 0.5f + 6.f)
        : FVector(0.f, 0.f, 18.f));

    ApplyStateLook();
}

void AFacilityInteractionPoint::SetPartColour(UStaticMeshComponent* Part, const FLinearColor& Colour)
{
    if (!Part)
    {
        return;
    }
    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0));
    if (!MID)
    {
        MID = Part->CreateAndSetMaterialInstanceDynamic(0);
    }
    if (MID)
    {
        MID->SetVectorParameterValue(TEXT("Color"), Colour);   // BasicShapeMaterial's parameter
    }
}

void AFacilityInteractionPoint::ApplyStateLook()
{
    FLinearColor LightColour(1.f, 0.18f, 0.11f);
    FLinearColor ScreenColour(0.3f, 0.02f, 0.02f);
    float Intensity = 60.f;

    switch (PointState)
    {
    case EFacilityPointState::Current:
        LightColour = FLinearColor(1.f, 0.55f, 0.08f);
        ScreenColour = FLinearColor(0.75f, 0.38f, 0.04f);
        Intensity = 200.f;
        break;
    case EFacilityPointState::Done:
        LightColour = FLinearColor(0.12f, 0.85f, 0.27f);
        ScreenColour = FLinearColor(0.04f, 0.5f, 0.14f);
        Intensity = 140.f;
        break;
    default:
        break;
    }

    if (Indicator)
    {
        Indicator->SetLightColor(LightColour);
        Indicator->SetIntensity(Intensity);
    }
    SetPartColour(Screen, ScreenColour);

    const bool bDone = PointState == EFacilityPointState::Done;
    if (Action == EFacilityAction::AccessCard && Detail)
    {
        Detail->SetVisibility(bShowPanel && !bDone);        // the card's in your pocket now
    }
    if (Action == EFacilityAction::RestorePower && Detail)
    {
        const FDeviceLook Look = LookFor(Action);
        Detail->SetRelativeLocation(Look.DetailCentre + FVector(0.f, 0.f, bDone ? 20.f : 0.f));   // lever up = on
    }
    SetActorTickEnabled(PointState == EFacilityPointState::Current);
}

void AFacilityInteractionPoint::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // Slow amber pulse on the current objective, so you can spot it across a dark room
    PulseTime += DeltaSeconds;
    if (Indicator && PointState == EFacilityPointState::Current)
    {
        const float Wave = 0.5f + 0.5f * FMath::Sin(PulseTime * 3.5f);
        Indicator->SetIntensity(70.f + 230.f * Wave);
    }
}

void AFacilityInteractionPoint::SetPointState(EFacilityPointState NewState)
{
    if (PointState == NewState)
    {
        return;
    }
    PointState = NewState;
    PulseTime = 0.f;
    ApplyStateLook();
}

void AFacilityInteractionPoint::SetCompleted(bool bInCompleted)
{
    SetPointState(bInCompleted ? EFacilityPointState::Done : EFacilityPointState::Locked);
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

FVector AFacilityInteractionPoint::GetDeviceLocation() const
{
    return PanelRoot ? PanelRoot->GetComponentLocation() : GetActorLocation();
}

AFacilityMissionDirector* AFacilityInteractionPoint::FindDirector() const
{
    if (AFacilityMissionDirector* Known = CachedDirector.Get())
    {
        return Known;
    }
    AFacilityMissionDirector* Found = AFacilityMissionDirector::Get(this);
    const_cast<AFacilityInteractionPoint*>(this)->CachedDirector = Found;
    return Found;
}

FString AFacilityInteractionPoint::GetPrompt(const AMyFPSPawn* User) const
{
    if (const AFacilityMissionDirector* Director = FindDirector())
    {
        return Director->GetPromptFor(this);
    }
    return Label + TEXT("  (offline)");
}

void AFacilityInteractionPoint::Interact(AMyFPSPawn* User)
{
    if (AFacilityMissionDirector* Director = FindDirector())
    {
        Director->HandleInteraction(this, User);
    }
}
