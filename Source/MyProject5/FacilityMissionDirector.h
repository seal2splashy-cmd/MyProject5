// FacilityMissionDirector.h
// Runs the A-07 facility mission: access card -> backup power -> security door -> decontamination ->
// observation log -> containment inspection (breach!) -> escape to the extraction beacon.
//
// - Every step is an AFacilityInteractionPoint: use it with F (the normal "[F] ..." prompt) or G.
// - The HUD (AMyHUD) shows the objective, mission messages and the "extracted" screen.
// - Checkpoints (UAHMissionProgress): die and you come back where you finished the last step.
// - Set pieces: powered lights flicker on, the security door grinds up into the ceiling, the airlock's
//   inner door is locked until decon finishes, and inspecting A-07 sets off the breach - alarm,
//   the wing's power dies, emergency lights pulse and a Gaunt is let out behind you.
//
// Level tags it looks for (Tools/build_a07_expanded_ue58.py + Tools/setup_a07_playable_ue58.py add them):
//   AH_PoweredLight        off until power is restored, die in the breach
//   AH_EmergencyLight      red lights that pulse once the alarm goes
//   AH_FlickerLight        flickers now and then, for atmosphere
//   AH_RestrictedVisualDoor  the security door slab that lifts away
//   AH_DeconLockedDoor     AAHDoor(s) unlocked + opened when decontamination finishes
//   AH_ExtractionDoor      AAHDoor(s) opened when you signal extraction
//   AH_MachineHum          generators / machinery that start rumbling when power is restored
//
// Powered lights buzz quietly while they're on (SW_LightHum_Loop) and go silent when the power dies.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FacilityInteractionPoint.h"
#include "FacilityMissionDirector.generated.h"

class AMyFPSPawn;
class UAudioComponent;
class ULightComponent;

/** One line of mission text on the HUD (plain C++, not reflected). */
struct FAHMissionMessage
{
    FString Text;
    FLinearColor Colour = FLinearColor::White;
    float Duration = 3.5f;
};

enum class EAHMissionLightKind : uint8 { Powered, Emergency, Flicker };

/** A light the director switches / flickers (plain C++, not reflected). */
struct FAHManagedLight
{
    TWeakObjectPtr<ULightComponent> Light;
    EAHMissionLightKind Kind = EAHMissionLightKind::Powered;
    float BaseIntensity = 1000.f;
    bool bTargetOn = false;
    float FxTime = -1.f;       // >= 0 while switching on/off (flickering)
    float FxDuration = 0.f;
    float NextFlicker = 0.f;   // ambient flicker countdown
    float FlickerLeft = 0.f;
    TWeakObjectPtr<UAudioComponent> Hum;   // fluorescent buzz while a powered light is on
};

UCLASS()
class MYPROJECT5_API AFacilityMissionDirector : public AActor
{
    GENERATED_BODY()

public:
    AFacilityMissionDirector();

    virtual void Tick(float DeltaSeconds) override;

    /** The director in this level, or nullptr. */
    static AFacilityMissionDirector* Get(const UObject* WorldContext);

    // ---- used by AFacilityInteractionPoint
    FString GetPromptFor(const AFacilityInteractionPoint* Point) const;
    void HandleInteraction(AFacilityInteractionPoint* Point, AMyFPSPawn* User);

    // ---- used by AMyHUD
    FString GetObjectiveText() const;
    /** Seconds since the objective last changed (the HUD flashes a new one). */
    float GetObjectiveAge() const;
    /** The mission message being shown now; false if none. Alpha fades it in and out. */
    bool GetCurrentMessage(FString& OutText, FLinearColor& OutColour, float& OutAlpha) const;
    bool IsMissionComplete() const { return bMissionComplete; }
    float GetMissionTime() const { return MissionTime; }
    float GetRestartIn() const;
    int32 GetDeaths() const { return DeathsThisRun; }
    bool IsBreached() const { return bBreached; }
    bool ShowsHUD() const { return bShowPrototypeHUD; }

    // ---- settings
    /** Let a Gaunt out of the chamber when you inspect A-07. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bSpawnGauntAfterContainment = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    FVector GauntSpawnLocation = FVector(7900.f, -1350.f, 110.f);

    /** Objective + messages on the HUD. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bShowPrototypeHUD = true;

    /** Also print messages with GEngine debug text (old behaviour). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bDebugMessages = false;

    /** G as a second "use" key. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bBindGKey = true;

    /** Come back at the last finished step when you die. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    bool bUseCheckpoints = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (ClampMin = 0))
    float DeconDuration = 5.f;

    /** How far the security door slab lifts (cm) and how long it takes. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    float RestrictedDoorLift = 310.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (ClampMin = 0.2))
    float RestrictedDoorOpenTime = 2.6f;

    /** Fluorescent buzz under each powered light (0 = off). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Ambience", meta = (ClampMin = 0))
    float LightHumVolume = 0.3f;

    /** Generator rumble from AH_MachineHum actors once power is back (0 = off). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Ambience", meta = (ClampMin = 0))
    float MachineHumVolume = 0.8f;

    /** How far away the breach alarm is heard (by you and by enemies). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    float AlarmRange = 6000.f;

    /** Seconds on the "extracted" screen before the level restarts. 0 = never. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (ClampMin = 0))
    float RestartDelay = 12.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Steps finished, 0..7. A point is done when (int)Action < Stage, current when ==. */
    int32 Stage = 0;
    bool bInitialised = false;
    bool bBusy = false;
    float BusyLeft = 0.f;
    int32 BusyShownSecond = -1;
    TWeakObjectPtr<AFacilityInteractionPoint> BusyPoint;

    bool bBreached = false;
    bool bGauntSpawned = false;
    bool bMissionComplete = false;
    float CompleteTime = 0.f;
    float MissionTime = 0.f;
    float ObjectiveChangedAt = 0.f;
    int32 DeathsThisRun = 0;
    float LightClock = 0.f;

    // security door lift
    TArray<TPair<TWeakObjectPtr<AActor>, FVector>> DoorSlabs;
    TWeakObjectPtr<AFacilityInteractionPoint> DoorPoint;
    float DoorAlpha = -1.f;     // -1 idle, 0..1 lifting, 2 open
    bool bDoorBlockerOpened = false;

    TArray<FAHMissionMessage> Messages;
    float MessageAge = 0.f;

    TArray<TWeakObjectPtr<AFacilityInteractionPoint>> Points;
    TArray<FAHManagedLight> Lights;

    UPROPERTY()
    UAudioComponent* AlarmAudio = nullptr;

    TArray<TWeakObjectPtr<AActor>> Machines;
    TArray<TWeakObjectPtr<UAudioComponent>> MachineAudio;
    bool bMachinesRunning = false;

    void Initialise();
    void RestoreCheckpoint();
    void ApplyStageInstant(int32 InStage);
    void CompleteStep(AFacilityInteractionPoint* Point);
    void RefreshPointStates();
    void SaveCheckpoint();

    void GInteract();
    AFacilityInteractionPoint* FindTarget() const;
    FString Objective() const;
    FString Requirement(EFacilityAction Which) const;
    FString Verb(EFacilityAction Which) const;

    void PushMessage(const FString& Text, const FLinearColor& Colour, float Duration = 3.5f, bool bClearQueue = false);
    /** Old name kept: shows a message. */
    void Display(const FString& Text, const FColor& Color = FColor::Cyan);

    void SetPoweredLights(bool bOn, bool bInstant = false);
    void TickLights(float DeltaSeconds);
    void SetLightHum(FAHManagedLight& M, bool bOn);
    void StartMachines(bool bInstant);
    void StartRestrictedDoor(AFacilityInteractionPoint* Point, bool bInstant);
    void TickRestrictedDoor(float DeltaSeconds);
    void OpenRestrictedVisualDoor();
    void ReleaseAirlock(bool bInstant);
    void StartBreach(bool bInstant);
    void SpawnExistingGaunt();
    void FinishMission();

    void PlayFacilitySound(const TCHAR* SoundName, const FVector& Where, float Volume, float AudibleRange) const;
    AMyFPSPawn* GetPlayerPawn() const;
};
