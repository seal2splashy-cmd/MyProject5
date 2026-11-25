// AHAmbienceComponent.h
// The building around you, reacting to how scared you are. On the player pawn.
//
//  - A low drone bed (SW_Amb_Drone_Loop) that swells with fear and after the breach.
//  - Distant one-shots from real positions 12-35 m away (so walls muffle and low-pass them through
//    AHRealism's attenuation, and the room reverb picks them up): metal groaning under load, pipes
//    knocking, settling creaks, heavy thuds somewhere, debris trickling. Rarer when you're calm,
//    much more frequent when you're afraid or the alarm is going.
//  - Stingers: the first time you actually see something tagged AH_Stinger (the dead operators,
//    Tools/place_story_ue58.py) - in view, in line of sight, close enough - a sharp sting plays and
//    the bodycam twitches. Once per thing, with a cooldown between stings.
// Missing sounds are skipped silently. No music anywhere: tension from the space itself.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHAmbienceComponent.generated.h"

class UAudioComponent;

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MYPROJECT5_API UAHAmbienceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAHAmbienceComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, Category = "Ambience")
    bool bEnabled = true;

    /** Drone volume when calm / extra at full fear. */
    UPROPERTY(EditAnywhere, Category = "Ambience")
    float DroneVolume = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Ambience")
    float DroneFearBoost = 0.55f;

    /** Seconds between distant sounds when calm (random in this range)... */
    UPROPERTY(EditAnywhere, Category = "Ambience")
    FVector2D CalmInterval = FVector2D(16.f, 38.f);

    /** ...multiplied by this at full fear / during the breach. */
    UPROPERTY(EditAnywhere, Category = "Ambience", meta = (ClampMin = 0.05, ClampMax = 1))
    float FearIntervalScale = 0.3f;

    /** How far away the distant sounds happen (cm). */
    UPROPERTY(EditAnywhere, Category = "Ambience")
    FVector2D EventDistance = FVector2D(1200.f, 3500.f);

    /** You have to be this close to a tagged thing to be stung by it (cm). */
    UPROPERTY(EditAnywhere, Category = "Ambience")
    float StingerRange = 1100.f;

    UPROPERTY(EditAnywhere, Category = "Ambience")
    float StingerCooldown = 12.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void PlayDistantEvent(const FVector& Listener);
    void CheckStingers(const FVector& Eye, const FVector& Forward);
    bool IsBreached();

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> Drone;

    TArray<TWeakObjectPtr<AActor>> StingerActors;
    TSet<TWeakObjectPtr<AActor>> Stung;
    float NextEventTime = 0.f;
    float NextStingerCheck = 0.f;
    float NextStingerRefresh = 0.f;
    float LastStingTime = -100.f;
    float BreachCheckTime = 0.f;
    bool bBreachedCached = false;
};
