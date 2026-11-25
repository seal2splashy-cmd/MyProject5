// AHDoor.cpp

#include "AHDoor.h"
#include "AEnemy.h"
#include "MyFPSPawn.h"
#include "RealismUtils.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    UStaticMeshComponent* MakePart(AActor* Owner, const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Cube)
    {
        UStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(Parent);
        if (Cube)
        {
            Part->SetStaticMesh(Cube);
        }
        Part->SetCollisionProfileName(TEXT("BlockAll"));
        Part->SetCanEverAffectNavigation(false);   // the NavMesh runs through doorways (unless locked)
        Part->SetMobility(EComponentMobility::Movable);
        return Part;
    }

    UMaterialInterface* LoadOptional(const TSoftObjectPtr<UMaterialInterface>& Soft)
    {
        if (Soft.IsNull())
        {
            return nullptr;
        }
        const FString Package = Soft.ToSoftObjectPath().GetLongPackageName();
        return FPackageName::DoesPackageExist(Package) ? Soft.LoadSynchronous() : nullptr;
    }
}

AAHDoor::AAHDoor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;   // only ticks while swinging

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cube = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    HingeLeft = CreateDefaultSubobject<USceneComponent>(TEXT("HingeLeft"));
    HingeLeft->SetupAttachment(Root);
    HingeRight = CreateDefaultSubobject<USceneComponent>(TEXT("HingeRight"));
    HingeRight->SetupAttachment(Root);

    LeafLeft = MakePart(this, TEXT("LeafLeft"), HingeLeft, Cube);
    LeafRight = MakePart(this, TEXT("LeafRight"), HingeRight, Cube);
    HandleLeft = MakePart(this, TEXT("HandleLeft"), HingeLeft, Cube);
    HandleRight = MakePart(this, TEXT("HandleRight"), HingeRight, Cube);
    HandleLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HandleRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    JambLeft = MakePart(this, TEXT("JambLeft"), Root, Cube);
    JambRight = MakePart(this, TEXT("JambRight"), Root, Cube);
    FrameHead = MakePart(this, TEXT("FrameHead"), Root, Cube);

    EnemyTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EnemyTrigger"));
    EnemyTrigger->SetupAttachment(Root);
    EnemyTrigger->SetCollisionProfileName(TEXT("Trigger"));
    EnemyTrigger->SetCanEverAffectNavigation(false);

    // Steel doors to match the facility; anything missing just keeps the engine grey.
    LeafMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/ForestResearch/Materials/M_FR_Steel.M_FR_Steel")));
    FrameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/ForestResearch/Materials/M_FR_Charcoal.M_FR_Charcoal")));
}

void AAHDoor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Layout();
    if (bStartOpen)
    {
        OpenAlpha = TargetAlpha = 1.f;
    }
    ApplySwing();
}

void AAHDoor::BeginPlay()
{
    Super::BeginPlay();
    Layout();
    OpenAlpha = TargetAlpha = bStartOpen ? 1.f : 0.f;
    ApplySwing();
    EnemyTrigger->OnComponentBeginOverlap.AddDynamic(this, &AAHDoor::OnTriggerBegin);
}

void AAHDoor::Layout()
{
    // Unit cube is 100 cm; parts are scaled boxes. Hinges sit at the doorway edges.
    const float LeafWidth = bDoubleDoor ? Width * 0.5f - 1.f : Width - 2.f;
    const float LeafHeight = Height - 1.f;
    const FVector LeafScale(Thickness / 100.f, LeafWidth / 100.f, LeafHeight / 100.f);

    HingeLeft->SetRelativeLocation(FVector(0.f, -Width * 0.5f, 0.f));
    HingeRight->SetRelativeLocation(FVector(0.f, Width * 0.5f, 0.f));

    // Left leaf runs +Y from its hinge, right leaf -Y from its hinge
    LeafLeft->SetRelativeLocation(FVector(0.f, LeafWidth * 0.5f + 1.f, LeafHeight * 0.5f + 0.5f));
    LeafLeft->SetRelativeScale3D(LeafScale);
    LeafRight->SetRelativeLocation(FVector(0.f, -(LeafWidth * 0.5f + 1.f), LeafHeight * 0.5f + 0.5f));
    LeafRight->SetRelativeScale3D(LeafScale);

    // Push-bar style handles across both faces, near the free edge
    const FVector HandleScale((Thickness + 9.f) / 100.f, 0.035f, 0.035f);
    HandleLeft->SetRelativeLocation(FVector(0.f, LeafWidth * 0.82f, 105.f));
    HandleLeft->SetRelativeScale3D(FVector(HandleScale.X, 0.22f, HandleScale.Z));
    HandleRight->SetRelativeLocation(FVector(0.f, -LeafWidth * 0.82f, 105.f));
    HandleRight->SetRelativeScale3D(FVector(HandleScale.X, 0.22f, HandleScale.Z));

    const bool bRight = bDoubleDoor;
    LeafRight->SetVisibility(bRight);
    HandleRight->SetVisibility(bRight);
    LeafRight->SetCollisionEnabled(bRight ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

    // Frame
    const float FrameDepth = Thickness + 10.f;
    JambLeft->SetRelativeLocation(FVector(0.f, -Width * 0.5f - 4.f, Height * 0.5f + 2.f));
    JambLeft->SetRelativeScale3D(FVector(FrameDepth / 100.f, 0.08f, (Height + 4.f) / 100.f));
    JambRight->SetRelativeLocation(FVector(0.f, Width * 0.5f + 4.f, Height * 0.5f + 2.f));
    JambRight->SetRelativeScale3D(FVector(FrameDepth / 100.f, 0.08f, (Height + 4.f) / 100.f));
    FrameHead->SetRelativeLocation(FVector(0.f, 0.f, Height + 4.f));
    FrameHead->SetRelativeScale3D(FVector(FrameDepth / 100.f, (Width + 16.f) / 100.f, 0.08f));
    for (UStaticMeshComponent* Part : { JambLeft, JambRight, FrameHead })
    {
        Part->SetVisibility(bShowFrame);
        Part->SetCollisionEnabled(bShowFrame ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    // Enemies coming through from either side
    EnemyTrigger->SetBoxExtent(FVector(110.f, Width * 0.5f + 20.f, Height * 0.5f));
    EnemyTrigger->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));

    // A locked door is a wall as far as the NavMesh is concerned (rebuild navigation after changing it)
    LeafLeft->SetCanEverAffectNavigation(bLocked);
    LeafRight->SetCanEverAffectNavigation(bLocked && bDoubleDoor);

    if (UMaterialInterface* Mat = LoadOptional(LeafMaterial))
    {
        LeafLeft->SetMaterial(0, Mat);
        LeafRight->SetMaterial(0, Mat);
    }
    if (UMaterialInterface* Mat = LoadOptional(FrameMaterial))
    {
        for (UStaticMeshComponent* Part : { JambLeft, JambRight, FrameHead, HandleLeft, HandleRight })
        {
            Part->SetMaterial(0, Mat);
        }
    }
}

void AAHDoor::ApplySwing()
{
    // Ease in and out like a real door on a closer
    const float Eased = FMath::SmoothStep(0.f, 1.f, OpenAlpha);
    const float Angle = OpenAngle * Eased * SwingSign;
    HingeLeft->SetRelativeRotation(FRotator(0.f, Angle, 0.f));
    HingeRight->SetRelativeRotation(FRotator(0.f, -Angle, 0.f));
}

void AAHDoor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, TargetAlpha, DeltaTime, SwingSpeed);
    ApplySwing();
    if (FMath::IsNearlyEqual(OpenAlpha, TargetAlpha))
    {
        SetActorTickEnabled(false);
    }
}

void AAHDoor::DoorSound(const TCHAR* Prefix, int32 Variants, float Volume, float NoiseRange, AActor* NoiseMaker)
{
    const FVector Where = GetActorLocation() + FVector(0.f, 0.f, 110.f);
    AHRealism::PlayAt(this, AHRealism::RandomVariant(Prefix, Variants), Where, Volume, FMath::FRandRange(0.94f, 1.06f), 2500.f);
    AEnemy::ReportNoise(this, Where, NoiseRange, NoiseMaker);
}

void AAHDoor::OpenAwayFrom(const FVector& From, bool bSlam, AActor* Opener)
{
    if (bLocked)
    {
        return;
    }
    // Which side are they on? Swing the other way.
    const FVector Local = GetActorTransform().InverseTransformPosition(From);
    const bool bWasShut = OpenAlpha < 0.05f;
    if (bWasShut)
    {
        SwingSign = Local.X >= 0.f ? 1.f : -1.f;
    }
    TargetAlpha = 1.f;
    SwingSpeed = (bSlam ? 3.f : 1.f) / FMath::Max(SwingTime, 0.05f);
    SetActorTickEnabled(true);

    if (bSlam)
        DoorSound(TEXT("SW_Door_Slam"), 1, 1.f, SlamNoiseRange, Opener);
    else
        DoorSound(TEXT("SW_Door_Open"), 2, 0.8f, OpenNoiseRange, Opener);
}

void AAHDoor::Close(AActor* Closer)
{
    TargetAlpha = 0.f;
    SwingSpeed = 1.f / FMath::Max(SwingTime, 0.05f);
    SetActorTickEnabled(true);
    DoorSound(TEXT("SW_Door_Close"), 1, 0.9f, CloseNoiseRange, Closer);
}

void AAHDoor::SetLocked(bool bInLocked)
{
    bLocked = bInLocked;
    LeafLeft->SetCanEverAffectNavigation(bLocked);
    LeafRight->SetCanEverAffectNavigation(bLocked && bDoubleDoor);
}

FString AAHDoor::GetPrompt(const AMyFPSPawn* User) const
{
    if (bLocked)
    {
        return LockedPrompt;
    }
    return IsOpen() ? TEXT("Close door") : TEXT("Open door");
}

void AAHDoor::Interact(AMyFPSPawn* User)
{
    if (!User)
    {
        return;
    }
    if (bLocked)
    {
        const float Now = GetWorld()->GetTimeSeconds();
        if (Now - LastRattleTime > 0.6f)
        {
            LastRattleTime = Now;
            DoorSound(TEXT("SW_Door_Locked"), 1, 0.9f, RattleNoiseRange, User);
        }
        return;
    }
    if (IsOpen())
    {
        Close(User);
    }
    else
    {
        // Barging through at a sprint slams it
        const bool bSlam = User->GetPlanarSpeed() > User->GetWalkSpeed() * 1.4f;
        OpenAwayFrom(User->GetActorLocation(), bSlam, User);
    }
}

void AAHDoor::OnTriggerBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bEnemiesCanOpen || bLocked || IsOpen())
    {
        return;
    }
    if (const AEnemy* Enemy = Cast<AEnemy>(OtherActor))
    {
        if (!Enemy->IsDead())
        {
            const bool bHunting = Enemy->GetState() == EAIState::Chase || Enemy->GetState() == EAIState::Attack;
            OpenAwayFrom(Enemy->GetActorLocation(), bHunting, OtherActor);
        }
    }
}
