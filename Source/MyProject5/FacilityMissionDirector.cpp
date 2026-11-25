// FacilityMissionDirector.cpp

#include "FacilityMissionDirector.h"
#include "AHMissionProgress.h"
#include "AHDoor.h"
#include "AEnemy.h"
#include "AGauntAnomaly.h"
#include "MyFPSPawn.h"
#include "RealismUtils.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/LightComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
    const FName TagPowered(TEXT("AH_PoweredLight"));
    const FName TagEmergency(TEXT("AH_EmergencyLight"));
    const FName TagFlicker(TEXT("AH_FlickerLight"));
    const FName TagSlab(TEXT("AH_RestrictedVisualDoor"));
    const FName TagDeconDoor(TEXT("AH_DeconLockedDoor"));
    const FName TagExtractionDoor(TEXT("AH_ExtractionDoor"));
    const FName TagGBound(TEXT("AH_FacilityGInputBound"));

    constexpr int32 StepCount = 7;   // EFacilityAction values, in mission order

    const FLinearColor ColInfo(0.55f, 0.85f, 1.f);
    const FLinearColor ColGood(0.35f, 1.f, 0.45f);
    const FLinearColor ColWarn(1.f, 0.8f, 0.25f);
    const FLinearColor ColBad(1.f, 0.25f, 0.2f);
    const FLinearColor ColLog(0.8f, 0.82f, 0.78f);
}

AFacilityMissionDirector::AFacilityMissionDirector()
{
    PrimaryActorTick.bCanEverTick = true;
}

AFacilityMissionDirector* AFacilityMissionDirector::Get(const UObject* WorldContext)
{
    UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
    if (!W)
    {
        return nullptr;
    }
    for (TActorIterator<AFacilityMissionDirector> It(W); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void AFacilityMissionDirector::BeginPlay()
{
    Super::BeginPlay();
    ObjectiveChangedAt = GetWorld()->GetTimeSeconds();
    // Everything else waits for the first Tick, so every point / door / light has run its own BeginPlay.
}

void AFacilityMissionDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AlarmAudio)
    {
        AlarmAudio->Stop();
        AlarmAudio = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

AMyFPSPawn* AFacilityMissionDirector::GetPlayerPawn() const
{
    return Cast<AMyFPSPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
}

// ------------------------------------------------------------------ setup

void AFacilityMissionDirector::Initialise()
{
    bInitialised = true;
    UWorld* W = GetWorld();

    for (TActorIterator<AFacilityInteractionPoint> It(W); It; ++It)
    {
        Points.Add(*It);
    }

    for (TActorIterator<AActor> It(W); It; ++It)
    {
        AActor* A = *It;
        if (!A)
        {
            continue;
        }
        const bool bPowered = A->ActorHasTag(TagPowered);
        const bool bEmergency = A->ActorHasTag(TagEmergency);
        const bool bFlicker = A->ActorHasTag(TagFlicker);
        if (bPowered || bEmergency || bFlicker)
        {
            if (ULightComponent* L = A->FindComponentByClass<ULightComponent>())
            {
                // Static lights can't change at runtime
                if (L->Mobility == EComponentMobility::Static)
                {
                    L->SetMobility(EComponentMobility::Movable);
                }
                FAHManagedLight M;
                M.Light = L;
                M.Kind = bPowered ? EAHMissionLightKind::Powered : (bEmergency ? EAHMissionLightKind::Emergency : EAHMissionLightKind::Flicker);
                M.BaseIntensity = L->Intensity;
                M.bTargetOn = !bPowered;
                M.NextFlicker = FMath::FRandRange(2.f, 9.f);
                if (bPowered)
                {
                    L->SetVisibility(false);
                }
                Lights.Add(M);
            }
        }
        if (A->ActorHasTag(TagSlab))
        {
            if (USceneComponent* R = A->GetRootComponent())
            {
                R->SetMobility(EComponentMobility::Movable);
            }
            DoorSlabs.Add(TPair<TWeakObjectPtr<AActor>, FVector>(A, A->GetActorLocation()));
        }
        if (A->ActorHasTag(TagDeconDoor))
        {
            if (AAHDoor* Door = Cast<AAHDoor>(A))
            {
                Door->SetLocked(true);
            }
        }
    }

    RestoreCheckpoint();
    RefreshPointStates();

    if (Stage == 0 && bShowPrototypeHUD)
    {
        PushMessage(TEXT("R.S.U. SITE A-17  |  Recording started"), ColInfo, 3.f);
        PushMessage(TEXT("F use   |   T torch   |   R reload"), ColLog, 4.f);
    }
}

void AFacilityMissionDirector::RestoreCheckpoint()
{
    if (!bUseCheckpoints)
    {
        return;
    }
    UGameInstance* GI = GetGameInstance();
    UAHMissionProgress* Progress = GI ? GI->GetSubsystem<UAHMissionProgress>() : nullptr;
    if (!Progress)
    {
        return;
    }
    int32 SavedStage = 0;
    FVector Where = FVector::ZeroVector;
    float Yaw = 0.f;
    float Elapsed = 0.f;
    if (!Progress->GetCheckpoint(UGameplayStatics::GetCurrentLevelName(this), SavedStage, Where, Yaw, Elapsed) || SavedStage <= 0)
    {
        return;
    }
    Progress->AddDeath();
    DeathsThisRun = Progress->GetDeaths();
    MissionTime = Elapsed;
    ApplyStageInstant(SavedStage);

    if (AMyFPSPawn* Pawn = GetPlayerPawn())
    {
        Pawn->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
        if (AController* C = Pawn->GetController())
        {
            C->SetControlRotation(FRotator(0.f, Yaw, 0.f));
        }
    }
    PushMessage(TEXT("SIGNAL RESTORED  |  Last checkpoint"), ColInfo, 3.f, true);
}

void AFacilityMissionDirector::ApplyStageInstant(int32 InStage)
{
    Stage = FMath::Clamp(InStage, 0, StepCount);
    const int32 PowerDone = static_cast<int32>(EFacilityAction::RestorePower) + 1;
    const int32 DoorDone = static_cast<int32>(EFacilityAction::RestrictedDoor) + 1;
    const int32 DeconDone = static_cast<int32>(EFacilityAction::Decontamination) + 1;
    const int32 BreachDone = static_cast<int32>(EFacilityAction::ContainmentConsole) + 1;

    if (Stage >= PowerDone)
    {
        SetPoweredLights(true, true);
    }
    if (Stage >= DoorDone)
    {
        for (const TWeakObjectPtr<AFacilityInteractionPoint>& P : Points)
        {
            if (P.IsValid() && P->Action == EFacilityAction::RestrictedDoor)
            {
                StartRestrictedDoor(P.Get(), true);
            }
        }
        if (DoorSlabs.Num() > 0 && !DoorPoint.IsValid())
        {
            StartRestrictedDoor(nullptr, true);
        }
    }
    if (Stage >= DeconDone)
    {
        ReleaseAirlock(true);
    }
    if (Stage >= BreachDone)
    {
        StartBreach(true);
    }
}

// ------------------------------------------------------------------ tick

void AFacilityMissionDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    AMyFPSPawn* Pawn = GetPlayerPawn();
    if (!bInitialised)
    {
        if (!Pawn)
        {
            return;     // wait for the player to be spawned + possessed
        }
        Initialise();
    }

    // G as a second use key (the pawn makes its InputComponent when it's possessed)
    if (bBindGKey && Pawn && Pawn->InputComponent && !Pawn->Tags.Contains(TagGBound))
    {
        Pawn->InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AFacilityMissionDirector::GInteract);
        Pawn->Tags.Add(TagGBound);
    }

    if (!bMissionComplete)
    {
        MissionTime += DeltaSeconds;
    }

    // Messages: show the head of the queue, then the next
    if (Messages.Num() > 0)
    {
        MessageAge += DeltaSeconds;
        if (MessageAge >= Messages[0].Duration)
        {
            Messages.RemoveAt(0);
            MessageAge = 0.f;
        }
    }

    // Decontamination cycle
    if (bBusy)
    {
        BusyLeft -= DeltaSeconds;
        const int32 Second = FMath::CeilToInt(FMath::Max(BusyLeft, 0.f));
        if (Second != BusyShownSecond && Second > 0)
        {
            BusyShownSecond = Second;
            PushMessage(FString::Printf(TEXT("DECONTAMINATION CYCLE  |  %d"), Second), ColWarn, 0.95f, true);
        }
        if (BusyLeft <= 0.f)
        {
            bBusy = false;
            AFacilityInteractionPoint* Point = BusyPoint.Get();
            ReleaseAirlock(false);
            PushMessage(TEXT("DECONTAMINATION COMPLETE  |  Inner airlock door released"), ColGood, 3.5f, true);
            CompleteStep(Point);
        }
    }

    TickRestrictedDoor(DeltaSeconds);
    TickLights(DeltaSeconds);

    // Restart after the extraction screen
    if (bMissionComplete && RestartDelay > 0.f && GetRestartIn() <= 0.f)
    {
        bMissionComplete = false;   // only once
        UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
    }
}

// ------------------------------------------------------------------ interaction

FString AFacilityMissionDirector::Verb(EFacilityAction Which) const
{
    switch (Which)
    {
    case EFacilityAction::AccessCard:         return TEXT("Take maintenance access card");
    case EFacilityAction::RestorePower:       return TEXT("Restore backup power");
    case EFacilityAction::RestrictedDoor:     return TEXT("Swipe access card");
    case EFacilityAction::Decontamination:    return TEXT("Start decontamination cycle");
    case EFacilityAction::ObservationConsole: return TEXT("Read observation log");
    case EFacilityAction::ContainmentConsole: return TEXT("Inspect A-07 containment");
    case EFacilityAction::Extraction:         return TEXT("Signal for extraction");
    default:                                  return TEXT("Use");
    }
}

FString AFacilityMissionDirector::Requirement(EFacilityAction Which) const
{
    switch (Which)
    {
    case EFacilityAction::RestorePower:       return TEXT("needs maintenance credentials");
    case EFacilityAction::RestrictedDoor:     return TEXT("no power / no access card");
    case EFacilityAction::Decontamination:    return TEXT("restricted access still locked");
    case EFacilityAction::ObservationConsole: return TEXT("airlock procedure incomplete");
    case EFacilityAction::ContainmentConsole: return TEXT("review the observation log first");
    case EFacilityAction::Extraction:         return TEXT("containment inspection still required");
    default:                                  return TEXT("unavailable");
    }
}

FString AFacilityMissionDirector::GetPromptFor(const AFacilityInteractionPoint* Point) const
{
    if (!Point || bMissionComplete)
    {
        return FString();
    }
    const int32 Index = static_cast<int32>(Point->Action);
    if (Index < Stage)
    {
        return FString();     // done: nothing to do here any more
    }
    if (bBusy && BusyPoint.Get() == Point)
    {
        return TEXT("Decontamination running...");
    }
    if (Index > Stage)
    {
        return Verb(Point->Action) + TEXT("  -  ") + Requirement(Point->Action);
    }
    return Verb(Point->Action);
}

void AFacilityMissionDirector::GInteract()
{
    if (AFacilityInteractionPoint* Point = FindTarget())
    {
        HandleInteraction(Point, GetPlayerPawn());
    }
}

AFacilityInteractionPoint* AFacilityMissionDirector::FindTarget() const
{
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !GetPlayerPawn())
    {
        return nullptr;
    }
    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye, View);
    const FVector Forward = View.Vector();
    AFacilityInteractionPoint* Best = nullptr;
    float BestScore = -1.f;

    for (const TWeakObjectPtr<AFacilityInteractionPoint>& Weak : Points)
    {
        AFacilityInteractionPoint* Point = Weak.Get();
        if (!Point || Point->IsCompleted())
        {
            continue;
        }
        const FVector To = Point->GetDeviceLocation() - Eye;
        const float Distance = To.Size();
        if (Distance > Point->ReachDistance || Distance < 1.f)
        {
            continue;
        }
        const float Facing = FVector::DotProduct(Forward, To / Distance);
        if (Facing < 0.58f)
        {
            continue;
        }
        const float Score = Facing * 2.f - Distance / Point->ReachDistance;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Point;
        }
    }
    return Best;
}

void AFacilityMissionDirector::HandleInteraction(AFacilityInteractionPoint* Point, AMyFPSPawn* User)
{
    if (!Point || bMissionComplete || bBusy)
    {
        return;
    }
    const int32 Index = static_cast<int32>(Point->Action);
    const FVector Where = Point->GetDeviceLocation();

    if (Index < Stage)
    {
        return;     // already done
    }
    if (Index > Stage)
    {
        PlayFacilitySound(TEXT("SW_Terminal_Deny_01"), Where, 0.8f, 900.f);
        const bool bHard = Point->Action == EFacilityAction::RestrictedDoor;
        const FString Why = Requirement(Point->Action);
        PushMessage(bHard ? FString(TEXT("ACCESS DENIED  |  ")) + Why : Why, bHard ? ColBad : ColWarn, 2.5f, true);
        return;
    }

    switch (Point->Action)
    {
    case EFacilityAction::AccessCard:
        PlayFacilitySound(TEXT("SW_Pickup_Gear_01"), Where, 0.8f, 600.f);
        PushMessage(TEXT("ACCESS CARD COLLECTED  |  Maintenance level 2"), ColGood, 3.5f, true);
        CompleteStep(Point);
        break;

    case EFacilityAction::RestorePower:
        PlayFacilitySound(TEXT("SW_Power_On_01"), Where, 1.f, 3000.f);
        AEnemy::ReportNoise(this, Where, 1800.f, User);     // a generator kicking in is loud
        SetPoweredLights(true);
        PushMessage(TEXT("BACKUP POWER ONLINE  |  Security access restored"), ColGood, 3.5f, true);
        CompleteStep(Point);
        break;

    case EFacilityAction::RestrictedDoor:
        PlayFacilitySound(TEXT("SW_Terminal_Accept_01"), Where, 0.8f, 800.f);
        StartRestrictedDoor(Point, false);
        PushMessage(TEXT("RESTRICTED ACCESS UNLOCKED"), ColGood, 3.f, true);
        CompleteStep(Point);
        break;

    case EFacilityAction::Decontamination:
        PlayFacilitySound(TEXT("SW_Terminal_Accept_01"), Where, 0.8f, 800.f);
        PlayFacilitySound(TEXT("SW_Decon_Hiss_01"), Where, 1.f, 1500.f);
        AEnemy::ReportNoise(this, Where, 900.f, User);
        bBusy = DeconDuration > 0.f;
        BusyLeft = DeconDuration;
        BusyShownSecond = -1;
        BusyPoint = Point;
        if (!bBusy)
        {
            ReleaseAirlock(false);
            CompleteStep(Point);
        }
        break;

    case EFacilityAction::ObservationConsole:
        PlayFacilitySound(TEXT("SW_Terminal_Accept_01"), Where, 0.7f, 700.f);
        PushMessage(TEXT("R.S.U. LOG 03/11  |  Chamber A-07 pressure irregularity detected"), ColLog, 4.f, true);
        PushMessage(TEXT("LOG 07/11  |  Movement inside the vessel. Requesting transfer"), ColLog, 4.f);
        PushMessage(TEXT("LOG 09/11  |  [DATA CORRUPTED]  ...it knows when the lights are on"), ColBad, 4.5f);
        CompleteStep(Point);
        break;

    case EFacilityAction::ContainmentConsole:
        PlayFacilitySound(TEXT("SW_Terminal_Accept_01"), Where, 0.7f, 700.f);
        CompleteStep(Point);
        StartBreach(false);
        break;

    case EFacilityAction::Extraction:
        PlayFacilitySound(TEXT("SW_Terminal_Accept_01"), Where, 0.9f, 900.f);
        CompleteStep(Point);
        FinishMission();
        break;
    }
}

void AFacilityMissionDirector::CompleteStep(AFacilityInteractionPoint* Point)
{
    if (Point && static_cast<int32>(Point->Action) != Stage)
    {
        return;
    }
    Stage = FMath::Min(Stage + 1, StepCount);
    ObjectiveChangedAt = GetWorld()->GetTimeSeconds();
    RefreshPointStates();
    SaveCheckpoint();
}

void AFacilityMissionDirector::RefreshPointStates()
{
    for (const TWeakObjectPtr<AFacilityInteractionPoint>& Weak : Points)
    {
        if (AFacilityInteractionPoint* Point = Weak.Get())
        {
            const int32 Index = static_cast<int32>(Point->Action);
            Point->SetPointState(Index < Stage ? EFacilityPointState::Done
                : (Index == Stage ? EFacilityPointState::Current : EFacilityPointState::Locked));
        }
    }
}

void AFacilityMissionDirector::SaveCheckpoint()
{
    if (!bUseCheckpoints || Stage >= StepCount)
    {
        return;
    }
    UGameInstance* GI = GetGameInstance();
    UAHMissionProgress* Progress = GI ? GI->GetSubsystem<UAHMissionProgress>() : nullptr;
    const AMyFPSPawn* Pawn = GetPlayerPawn();
    if (!Progress || !Pawn || Pawn->IsDead())
    {
        return;
    }
    const AController* C = Pawn->GetController();
    const float Yaw = C ? C->GetControlRotation().Yaw : Pawn->GetActorRotation().Yaw;
    Progress->SaveCheckpoint(UGameplayStatics::GetCurrentLevelName(this), Stage, Pawn->GetActorLocation(), Yaw, MissionTime);
}

// ------------------------------------------------------------------ text for the HUD

FString AFacilityMissionDirector::Objective() const
{
    if (bBusy) return TEXT("Wait for the decontamination cycle");
    switch (Stage)
    {
    case 0: return TEXT("Find the maintenance access card in storage");
    case 1: return TEXT("Restore backup power at the generators");
    case 2: return TEXT("Unlock the security door at the rear corridor");
    case 3: return TEXT("Run the decontamination airlock");
    case 4: return TEXT("Read the observation terminal");
    case 5: return TEXT("Inspect the A-07 containment system");
    case 6: return TEXT("Get out - maintenance tunnel to the extraction beacon");
    default: return TEXT("Extracted");
    }
}

FString AFacilityMissionDirector::GetObjectiveText() const
{
    return Objective();
}

float AFacilityMissionDirector::GetObjectiveAge() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() - ObjectiveChangedAt : 100.f;
}

bool AFacilityMissionDirector::GetCurrentMessage(FString& OutText, FLinearColor& OutColour, float& OutAlpha) const
{
    if (Messages.Num() == 0)
    {
        return false;
    }
    const FAHMissionMessage& M = Messages[0];
    OutText = M.Text;
    OutColour = M.Colour;
    const float In = FMath::Clamp(MessageAge / 0.15f, 0.f, 1.f);
    const float Out = FMath::Clamp((M.Duration - MessageAge) / 0.5f, 0.f, 1.f);
    OutAlpha = FMath::Min(In, Out);
    return true;
}

float AFacilityMissionDirector::GetRestartIn() const
{
    if (!bMissionComplete || RestartDelay <= 0.f || !GetWorld())
    {
        return -1.f;
    }
    return FMath::Max(0.f, RestartDelay - (GetWorld()->GetTimeSeconds() - CompleteTime));
}

void AFacilityMissionDirector::PushMessage(const FString& Text, const FLinearColor& Colour, float Duration, bool bClearQueue)
{
    UE_LOG(LogTemp, Display, TEXT("[Anomaly Hunter Mission] %s"), *Text);
    if (bDebugMessages && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, Duration, Colour.ToFColor(true), Text);
    }
    if (bClearQueue)
    {
        Messages.Reset();
        MessageAge = 0.f;
    }
    FAHMissionMessage M;
    M.Text = Text;
    M.Colour = Colour;
    M.Duration = FMath::Max(Duration, 0.3f);
    Messages.Add(M);
}

void AFacilityMissionDirector::Display(const FString& Text, const FColor& Color)
{
    PushMessage(Text, FLinearColor(Color));
}

// ------------------------------------------------------------------ set pieces

void AFacilityMissionDirector::PlayFacilitySound(const TCHAR* SoundName, const FVector& Where, float Volume, float AudibleRange) const
{
    AHRealism::PlayAt(this, AHRealism::GeneratedSound(SoundName), Where, Volume, 1.f, AudibleRange);
}

void AFacilityMissionDirector::SetPoweredLights(bool bOn, bool bInstant)
{
    for (FAHManagedLight& M : Lights)
    {
        if (M.Kind != EAHMissionLightKind::Powered)
        {
            continue;
        }
        M.bTargetOn = bOn;
        if (bInstant)
        {
            M.FxTime = -1.f;
            if (ULightComponent* L = M.Light.Get())
            {
                L->SetVisibility(bOn);
                L->SetIntensity(M.BaseIntensity);
            }
        }
        else
        {
            // Fluorescent tubes don't just come on: each one stutters for a moment, not in sync
            M.FxTime = 0.f;
            M.FxDuration = FMath::FRandRange(0.6f, bOn ? 1.8f : 2.6f);
        }
    }
}

void AFacilityMissionDirector::TickLights(float DeltaSeconds)
{
    LightClock += DeltaSeconds;
    for (FAHManagedLight& M : Lights)
    {
        ULightComponent* L = M.Light.Get();
        if (!L)
        {
            continue;
        }
        switch (M.Kind)
        {
        case EAHMissionLightKind::Powered:
            if (M.FxTime >= 0.f)
            {
                M.FxTime += DeltaSeconds;
                const float P = FMath::Clamp(M.FxTime / FMath::Max(M.FxDuration, 0.01f), 0.f, 1.f);
                // Switching on: more and more "on" frames. Dying: fewer and dimmer.
                const float OnChance = M.bTargetOn ? 0.15f + 0.8f * P : 0.85f - 0.8f * P;
                const bool bLit = FMath::FRand() < OnChance;
                L->SetVisibility(bLit);
                L->SetIntensity(M.BaseIntensity * (M.bTargetOn ? 1.f : FMath::Lerp(1.f, 0.3f, P)));
                if (P >= 1.f)
                {
                    M.FxTime = -1.f;
                    L->SetVisibility(M.bTargetOn);
                    L->SetIntensity(M.BaseIntensity);
                }
            }
            break;

        case EAHMissionLightKind::Emergency:
            if (bBreached)
            {
                // Pulsing like a rotating beacon in time with the alarm
                const float Wave = 0.5f + 0.5f * FMath::Sin(LightClock * 5.2f);
                L->SetIntensity(M.BaseIntensity * (0.25f + 1.35f * Wave * Wave));
            }
            break;

        case EAHMissionLightKind::Flicker:
            if (M.FlickerLeft > 0.f)
            {
                M.FlickerLeft -= DeltaSeconds;
                L->SetIntensity(M.BaseIntensity * (FMath::FRand() < 0.55f ? 1.f : FMath::FRandRange(0.02f, 0.3f)));
                if (M.FlickerLeft <= 0.f)
                {
                    L->SetIntensity(M.BaseIntensity);
                    M.NextFlicker = FMath::FRandRange(bBreached ? 1.f : 3.f, bBreached ? 4.f : 11.f);
                }
            }
            else
            {
                M.NextFlicker -= DeltaSeconds;
                if (M.NextFlicker <= 0.f)
                {
                    M.FlickerLeft = FMath::FRandRange(0.15f, 0.9f);
                }
            }
            break;
        }
    }
}

void AFacilityMissionDirector::StartRestrictedDoor(AFacilityInteractionPoint* Point, bool bInstant)
{
    if (Point)
    {
        DoorPoint = Point;
    }
    if (bInstant)
    {
        DoorAlpha = 1.f;
        TickRestrictedDoor(0.f);
        return;
    }
    if (DoorAlpha < 0.f)
    {
        DoorAlpha = 0.f;
        bDoorBlockerOpened = false;
        const FVector Where = DoorSlabs.Num() > 0 && DoorSlabs[0].Key.IsValid()
            ? DoorSlabs[0].Key->GetActorLocation()
            : (Point ? Point->GetActorLocation() : GetActorLocation());
        PlayFacilitySound(TEXT("SW_BlastDoor_Open_01"), Where, 1.f, 3500.f);
        AEnemy::ReportNoise(this, Where, 1500.f, GetPlayerPawn());
    }
}

void AFacilityMissionDirector::TickRestrictedDoor(float DeltaSeconds)
{
    if (DoorAlpha < 0.f || DoorAlpha > 1.5f)
    {
        return;     // idle, or finished (2)
    }
    DoorAlpha = FMath::Min(1.f, DoorAlpha + DeltaSeconds / FMath::Max(RestrictedDoorOpenTime, 0.1f));
    // Slow start (the motor taking the weight), steady lift, soft stop
    const float Eased = FMath::InterpEaseInOut(0.f, 1.f, DoorAlpha, 2.2f);
    for (const TPair<TWeakObjectPtr<AActor>, FVector>& Slab : DoorSlabs)
    {
        if (AActor* A = Slab.Key.Get())
        {
            A->SetActorLocation(Slab.Value + FVector(0.f, 0.f, RestrictedDoorLift * Eased));
        }
    }
    // Duck under it once it's above head height
    if (!bDoorBlockerOpened && Eased > 0.7f)
    {
        bDoorBlockerOpened = true;
        if (AFacilityInteractionPoint* P = DoorPoint.Get())
        {
            P->SetDoorOpen(true);
        }
    }
    if (DoorAlpha >= 1.f)
    {
        OpenRestrictedVisualDoor();
        DoorAlpha = 2.f;
    }
}

void AFacilityMissionDirector::OpenRestrictedVisualDoor()
{
    for (const TPair<TWeakObjectPtr<AActor>, FVector>& Slab : DoorSlabs)
    {
        if (AActor* A = Slab.Key.Get())
        {
            A->SetActorHiddenInGame(true);
            A->SetActorEnableCollision(false);
        }
    }
    if (AFacilityInteractionPoint* P = DoorPoint.Get())
    {
        P->SetDoorOpen(true);
    }
}

void AFacilityMissionDirector::ReleaseAirlock(bool bInstant)
{
    for (TActorIterator<AAHDoor> It(GetWorld()); It; ++It)
    {
        AAHDoor* Door = *It;
        if (!Door || !Door->ActorHasTag(TagDeconDoor))
        {
            continue;
        }
        Door->SetLocked(false);
        if (!bInstant)
        {
            // Opens away from the airlock side, with its own hinge sound
            const FVector From = BusyPoint.IsValid() ? BusyPoint->GetActorLocation() : GetActorLocation();
            Door->OpenAwayFrom(From, false, this);
        }
    }
}

void AFacilityMissionDirector::StartBreach(bool bInstant)
{
    if (bBreached)
    {
        return;
    }
    bBreached = true;

    // Where the alarm sounds from: the containment console if there is one
    FVector AlarmAt = GetActorLocation();
    for (const TWeakObjectPtr<AFacilityInteractionPoint>& P : Points)
    {
        if (P.IsValid() && P->Action == EFacilityAction::ContainmentConsole)
        {
            AlarmAt = P->GetActorLocation();
        }
    }

    if (USoundBase* Alarm = AHRealism::GeneratedSound(TEXT("SW_Alarm_Loop")))
    {
        AlarmAudio = UGameplayStatics::SpawnSoundAtLocation(this, Alarm, AlarmAt + FVector(0.f, 0.f, 500.f),
            FRotator::ZeroRotator, 0.7f, 1.f, 0.f, AHRealism::Attenuation(AlarmRange));
    }
    SetPoweredLights(false, bInstant);

    if (!bInstant)
    {
        PlayFacilitySound(TEXT("SW_Light_Pop_01"), AlarmAt + FVector(0.f, 0.f, 600.f), 1.f, 3000.f);
        AEnemy::ReportNoise(this, AlarmAt, AlarmRange * 0.8f, GetPlayerPawn());
        PushMessage(TEXT("!!  CONTAINMENT BREACH  |  A-07  !!"), ColBad, 3.5f, true);
        PushMessage(TEXT("Wing power failing. Get out through maintenance."), ColWarn, 4.f);
    }

    if (bSpawnGauntAfterContainment)
    {
        SpawnExistingGaunt();
    }
}

void AFacilityMissionDirector::SpawnExistingGaunt()
{
    if (bGauntSpawned)
    {
        return;
    }
    bGauntSpawned = true;
    AMyFPSPawn* Pawn = GetPlayerPawn();
    const FVector Target = Pawn ? Pawn->GetActorLocation() : GetActorLocation();
    const FRotator Facing(0.f, (Target - GauntSpawnLocation).Rotation().Yaw, 0.f);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AGauntAnomaly* Gaunt = GetWorld()->SpawnActor<AGauntAnomaly>(AGauntAnomaly::StaticClass(), GauntSpawnLocation, Facing, Params);
    if (!Gaunt)
    {
        PushMessage(TEXT("(Encounter skipped: couldn't spawn the Gaunt)"), ColWarn, 2.f);
        return;
    }
    // It knows roughly where you are: it heard the console
    if (Pawn)
    {
        Gaunt->HearNoise(Target, 4000.f, Pawn);
    }
}

void AFacilityMissionDirector::FinishMission()
{
    bMissionComplete = true;
    CompleteTime = GetWorld()->GetTimeSeconds();
    if (AlarmAudio)
    {
        AlarmAudio->FadeOut(3.f, 0.f);
    }
    for (TActorIterator<AAHDoor> It(GetWorld()); It; ++It)
    {
        if (*It && It->ActorHasTag(TagExtractionDoor))
        {
            It->SetLocked(false);
            It->OpenAwayFrom(GetPlayerPawn() ? GetPlayerPawn()->GetActorLocation() : GetActorLocation(), false, this);
        }
    }
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UAHMissionProgress* Progress = GI->GetSubsystem<UAHMissionProgress>())
        {
            DeathsThisRun = Progress->GetDeaths();
            Progress->Clear();
        }
    }
    PushMessage(TEXT("EXTRACTION CONFIRMED  |  Facility investigation successful"), ColGood, 6.f, true);
}
