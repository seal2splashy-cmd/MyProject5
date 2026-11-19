#include "FacilityMissionDirector.h"
#include "MyFPSPawn.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AFacilityMissionDirector::AFacilityMissionDirector()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AFacilityMissionDirector::BeginPlay()
{
    Super::BeginPlay();
    Display(TEXT("R.S.U. SITE A-17 | Mission started"), FColor::Cyan);
}

void AFacilityMissionDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // The pawn creates its InputComponent during possession; bind after it exists.
    static const FName BindingTag(TEXT("AH_FacilityGInputBound"));
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (AMyFPSPawn* Pawn = Cast<AMyFPSPawn>(PC->GetPawn()))
        {
            if (UInputComponent* Input = Pawn->InputComponent)
            {
                if (!Pawn->Tags.Contains(BindingTag))
                {
                    Input->BindKey(EKeys::G, IE_Pressed, this, &AFacilityMissionDirector::Interact);
                    Pawn->Tags.Add(BindingTag);
                    Display(TEXT("G = interact | F = torch | E = tilt"), FColor::Cyan);
                }
            }
        }
    }

    UpdateTime += DeltaSeconds;
    if (UpdateTime >= 2.5f)
    {
        UpdateTime = 0.f;
        if (bShowPrototypeHUD && !bEscaped)
        {
            Display(TEXT("OBJECTIVE: ") + Objective(), FColor::Cyan);
        }
    }
}

AFacilityInteractionPoint* AFacilityMissionDirector::FindTarget() const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    const AMyFPSPawn* Pawn = PC ? Cast<AMyFPSPawn>(PC->GetPawn()) : nullptr;
    if (!Pawn) return nullptr;

    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye, View);
    const FVector Forward = View.Vector();
    AFacilityInteractionPoint* Best = nullptr;
    float BestScore = -1.f;

    for (TActorIterator<AFacilityInteractionPoint> It(GetWorld()); It; ++It)
    {
        AFacilityInteractionPoint* Point = *It;
        if (!Point || Point->IsCompleted()) continue;
        const FVector To = Point->GetActorLocation() - Eye;
        const float Distance = To.Size();
        if (Distance > Point->ReachDistance || Distance < 1.f) continue;
        const float Facing = FVector::DotProduct(Forward, To / Distance);
        if (Facing < 0.58f) continue;
        const float Score = Facing * 2.f - Distance / Point->ReachDistance;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Point;
        }
    }
    return Best;
}

void AFacilityMissionDirector::Interact()
{
    if (bEscaped) return;
    AFacilityInteractionPoint* Point = FindTarget();
    if (!Point)
    {
        Display(TEXT("No interactable control in view (G)"), FColor::Silver);
        return;
    }

    switch (Point->Action)
    {
    case EFacilityAction::AccessCard:
        bHasCard = true;
        Point->SetCompleted(true);
        Display(TEXT("ACCESS CARD COLLECTED | Maintenance level 2"), FColor::Green);
        break;
    case EFacilityAction::RestorePower:
        if (!bHasCard) { Display(TEXT("Power console requires maintenance credentials"), FColor::Yellow); break; }
        bPower = true;
        Point->SetCompleted(true);
        SetPoweredLights(true);
        Display(TEXT("BACKUP POWER ONLINE | Security access restored"), FColor::Green);
        break;
    case EFacilityAction::RestrictedDoor:
        if (!bPower || !bHasCard) { Display(TEXT("ACCESS DENIED | Restore power and collect card"), FColor::Red); break; }
        bDoorOpen = true;
        Point->SetDoorOpen(true);
        OpenRestrictedVisualDoor();
        Display(TEXT("RESTRICTED ACCESS UNLOCKED"), FColor::Green);
        break;
    case EFacilityAction::Decontamination:
        if (!bDoorOpen) { Display(TEXT("Restricted access must be unlocked"), FColor::Yellow); break; }
        bDecontaminated = true;
        Point->SetCompleted(true);
        Display(TEXT("DECONTAMINATION COMPLETE | Airlock released"), FColor::Green);
        break;
    case EFacilityAction::ObservationConsole:
        if (!bDecontaminated) { Display(TEXT("Airlock procedure is incomplete"), FColor::Yellow); break; }
        bObservationRead = true;
        Point->SetCompleted(true);
        Display(TEXT("R.S.U. LOG: Chamber A-07 pressure irregularity detected"), FColor::Cyan);
        break;
    case EFacilityAction::ContainmentConsole:
        if (!bObservationRead) { Display(TEXT("Review observation logs first"), FColor::Yellow); break; }
        bContainmentInspected = true;
        Point->SetCompleted(true);
        Display(TEXT("A-07 INSPECTED | Return to emergency extraction"), FColor::Red);
        if (bSpawnGauntAfterContainment) SpawnExistingGaunt();
        break;
    case EFacilityAction::Extraction:
        if (!bContainmentInspected) { Display(TEXT("Containment inspection still required"), FColor::Yellow); break; }
        bEscaped = true;
        Point->SetCompleted(true);
        Display(TEXT("MISSION COMPLETE | Facility investigation successful"), FColor::Green);
        break;
    }
}

FString AFacilityMissionDirector::Objective() const
{
    if (!bHasCard) return TEXT("Find the maintenance access card in storage");
    if (!bPower) return TEXT("Restore backup power at the generator");
    if (!bDoorOpen) return TEXT("Unlock restricted access at the rear corridor");
    if (!bDecontaminated) return TEXT("Activate the decontamination airlock");
    if (!bObservationRead) return TEXT("Read the observation terminal");
    if (!bContainmentInspected) return TEXT("Inspect the A-07 containment system");
    if (!bEscaped) return TEXT("Return to the exterior extraction checkpoint");
    return TEXT("Completed");
}

FString AFacilityMissionDirector::Requirement(EFacilityAction Action) const
{
    switch (Action)
    {
    case EFacilityAction::RestorePower: return TEXT("Access card");
    case EFacilityAction::RestrictedDoor: return TEXT("Access card + backup power");
    case EFacilityAction::Decontamination: return TEXT("Restricted access");
    case EFacilityAction::ObservationConsole: return TEXT("Decontamination");
    case EFacilityAction::ContainmentConsole: return TEXT("Observation log");
    case EFacilityAction::Extraction: return TEXT("A-07 inspection");
    default: return TEXT("None");
    }
}

void AFacilityMissionDirector::Display(const FString& Text, const FColor& Color) const
{
    UE_LOG(LogTemp, Display, TEXT("[Anomaly Hunter Mission] %s"), *Text);
    if (GEngine && bShowPrototypeHUD)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, Color, Text);
    }
}

void AFacilityMissionDirector::SetPoweredLights(bool bOn)
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A = *It;
        if (!A || !A->ActorHasTag(FName(TEXT("AH_PoweredLight")))) continue;
        if (UPointLightComponent* Light = A->FindComponentByClass<UPointLightComponent>())
        {
            Light->SetVisibility(bOn);
        }
    }
}

void AFacilityMissionDirector::OpenRestrictedVisualDoor()
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A = *It;
        if (A && A->ActorHasTag(FName(TEXT("AH_RestrictedVisualDoor"))))
        {
            A->SetActorHiddenInGame(true);
            A->SetActorEnableCollision(false);
        }
    }
}

void AFacilityMissionDirector::SpawnExistingGaunt()
{
    if (bGauntSpawned) return;
    bGauntSpawned = true;
    // Opt-in only. Existing enemy C++ class, not a newly generated model.
    UClass* GauntClass = LoadClass<AActor>(nullptr, TEXT("/Script/MyProject5.GauntAnomaly"));
    if (!GauntClass)
    {
        Display(TEXT("Encounter skipped: GauntAnomaly class unavailable"), FColor::Yellow);
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    GetWorld()->SpawnActor<AActor>(GauntClass, GauntSpawnLocation, FRotator::ZeroRotator, Params);
}
