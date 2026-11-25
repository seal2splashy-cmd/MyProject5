// AHPickup.cpp

#include "AHPickup.h"
#include "AEnemy.h"
#include "MyFPSPawn.h"
#include "RealismUtils.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AAHPickup::AAHPickup()
{
    PrimaryActorTick.bCanEverTick = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
    CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Body->SetupAttachment(Root);
    Detail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Detail"));
    Detail->SetupAttachment(Root);

    for (UStaticMeshComponent* Part : { Body, Detail })
    {
        // Doesn't get in your way, but the interaction trace (Visibility) finds it
        Part->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Part->SetCollisionResponseToAllChannels(ECR_Ignore);
        Part->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Part->SetCanEverAffectNavigation(false);
        Part->SetGenerateOverlapEvents(false);
    }

    InteractRange = 190.f;
}

void AAHPickup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    BuildLook();
}

void AAHPickup::BuildLook()
{
    // Engine basic shapes: 100 cm cube / cylinder (cylinder axis = Z), origin at the centre.
    // The actor's origin is the bottom: place it on a floor or a table top.
    auto Shape = [](UStaticMeshComponent* Part, UStaticMesh* Mesh, const FVector& SizeCm, const FVector& Centre,
        const FRotator& Rot, const FLinearColor& Colour)
    {
        if (Mesh)
        {
            Part->SetStaticMesh(Mesh);
        }
        Part->SetRelativeScale3D(SizeCm / 100.f);
        Part->SetRelativeLocation(Centre);
        Part->SetRelativeRotation(Rot);
        if (UMaterialInstanceDynamic* MID = Part->CreateAndSetMaterialInstanceDynamic(0))
        {
            MID->SetVectorParameterValue(TEXT("Color"), Colour);   // BasicShapeMaterial's parameter
        }
    };

    switch (Type)
    {
    case EAHPickupType::Magazine:
        // A loaded pistol magazine lying flat (13 x 3.5 x 2.4 cm), brass of the top round showing
        Shape(Body, CubeMesh, FVector(13.f, 3.5f, 2.4f), FVector(0.f, 0.f, 1.2f), FRotator::ZeroRotator,
            FLinearColor(0.035f, 0.037f, 0.04f));
        Shape(Detail, CylinderMesh, FVector(0.9f, 0.9f, 2.6f), FVector(5.2f, 0.f, 1.2f), FRotator(90.f, 0.f, 0.f),
            FLinearColor(0.8f, 0.55f, 0.2f));
        break;

    case EAHPickupType::Bandage:
        // A rolled bandage on its side (7.5 cm across, 10 cm wide) with a red band
        Shape(Body, CylinderMesh, FVector(7.5f, 7.5f, 10.f), FVector(0.f, 0.f, 3.75f), FRotator(0.f, 0.f, 90.f),
            FLinearColor(0.85f, 0.83f, 0.78f));
        Shape(Detail, CylinderMesh, FVector(7.7f, 7.7f, 3.f), FVector(0.f, 0.f, 3.75f), FRotator(0.f, 0.f, 90.f),
            FLinearColor(0.7f, 0.1f, 0.08f));
        break;

    case EAHPickupType::Battery:
        // A boxy torch battery pack: yellow with a black end cap
        Shape(Body, CubeMesh, FVector(10.f, 5.5f, 3.2f), FVector(0.f, 0.f, 1.6f), FRotator::ZeroRotator,
            FLinearColor(0.9f, 0.65f, 0.05f));
        Shape(Detail, CubeMesh, FVector(3.f, 5.6f, 3.3f), FVector(3.6f, 0.f, 1.6f), FRotator::ZeroRotator,
            FLinearColor(0.03f, 0.03f, 0.03f));
        break;
    }
}

FString AAHPickup::GetPrompt(const AMyFPSPawn* User) const
{
    switch (Type)
    {
    case EAHPickupType::Magazine:
        if (User && !User->CanTakeMagazine()) return TEXT("Mag pouch full");
        return FString::Printf(TEXT("Take 9mm magazine (%d)"), Rounds);
    case EAHPickupType::Bandage:
        if (User && !User->CanUseBandage()) return TEXT("Bandage (not hurt)");
        return TEXT("Use bandage");
    case EAHPickupType::Battery:
        if (User && !User->CanTakeBattery()) return TEXT("Torch battery (torch is full)");
        return TEXT("Swap torch battery");
    }
    return FString();
}

void AAHPickup::Interact(AMyFPSPawn* User)
{
    if (!User)
    {
        return;
    }

    bool bUsed = false;
    const TCHAR* Sound = TEXT("SW_Pickup_Gear");
    switch (Type)
    {
    case EAHPickupType::Magazine: bUsed = User->TakeMagazine(Rounds); Sound = TEXT("SW_Pickup_Mag"); break;
    case EAHPickupType::Bandage:  bUsed = User->UseBandage(HealAmount); Sound = TEXT("SW_Pickup_Bandage"); break;
    case EAHPickupType::Battery:  bUsed = User->TakeBattery(BatteryAmount); break;
    }
    if (!bUsed)
    {
        return;
    }

    AHRealism::PlayAt(this, AHRealism::RandomVariant(Sound, 1), GetActorLocation(), 0.8f, FMath::FRandRange(0.95f, 1.05f), 1200.f);
    AEnemy::ReportNoise(this, GetActorLocation(), 200.f, User);   // rummaging is quiet but not silent
    Destroy();
}
