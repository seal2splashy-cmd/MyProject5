// AHAmbienceComponent.cpp

#include "AHAmbienceComponent.h"
#include "MyFPSPawn.h"
#include "BodycamLensComponent.h"
#include "FacilityMissionDirector.h"
#include "RealismUtils.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
    const FName TagStinger(TEXT("AH_Stinger"));

    struct FDistantSound
    {
        const TCHAR* Prefix;
        int32 Variants;
        float Weight;
        float Volume;
        float HeightMin;
        float HeightMax;
    };

    // What a large concrete-and-steel building does on its own
    const FDistantSound DistantSounds[] = {
        { TEXT("SW_Amb_MetalGroan"), 3, 3.f, 0.9f, 150.f, 500.f },   // steel under load, ductwork expanding
        { TEXT("SW_Amb_PipeKnock"),  3, 3.f, 0.8f, 200.f, 350.f },   // water hammer in the pipes
        { TEXT("SW_Amb_Creak"),      2, 2.f, 0.7f, 0.f,   300.f },   // the building settling
        { TEXT("SW_Amb_Thud"),       2, 1.5f, 1.f, 0.f,   200.f },   // something heavy, somewhere
        { TEXT("SW_Amb_Debris"),     2, 1.f, 0.6f, 200.f, 400.f },   // grit trickling from the ceiling
    };
}

UAHAmbienceComponent::UAHAmbienceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f;
}

void UAHAmbienceComponent::BeginPlay()
{
    Super::BeginPlay();
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    NextEventTime = Now + FMath::FRandRange(6.f, 14.f);
    if (bEnabled)
    {
        if (USoundBase* Bed = AHRealism::GeneratedSound(TEXT("SW_Amb_Drone_Loop")))
        {
            Drone = UGameplayStatics::SpawnSound2D(this, Bed, DroneVolume, 1.f, FMath::FRandRange(0.f, 8.f),
                nullptr, false, false);
        }
    }
}

void UAHAmbienceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Drone)
    {
        Drone->Stop();
        Drone = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

bool UAHAmbienceComponent::IsBreached()
{
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    if (Now >= BreachCheckTime)
    {
        BreachCheckTime = Now + 1.f;
        const AFacilityMissionDirector* Director = AFacilityMissionDirector::Get(this);
        bBreachedCached = Director && Director->IsBreached();
    }
    return bBreachedCached;
}

void UAHAmbienceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const AMyFPSPawn* Pawn = Cast<AMyFPSPawn>(GetOwner());
    if (!bEnabled || !Pawn)
    {
        return;
    }
    const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
    const float Fear = Pawn->IsDead() ? 0.f : Pawn->GetFear01();
    const bool bBreach = IsBreached();
    const float Tension = FMath::Clamp(Fear + (bBreach ? 0.35f : 0.f), 0.f, 1.f);

    if (Drone)
    {
        const float Want = DroneVolume + DroneFearBoost * Tension;
        Drone->SetVolumeMultiplier(FMath::FInterpTo(Drone->VolumeMultiplier, Want, DeltaTime, 0.6f));
        Drone->SetPitchMultiplier(1.f - 0.06f * Tension);      // the bed sinks as you get scared
    }

    const FVector Eye = Pawn->GetPawnViewLocation();
    if (Now >= NextEventTime)
    {
        PlayDistantEvent(Eye);
        const float Scale = FMath::Lerp(1.f, FearIntervalScale, Tension);
        NextEventTime = Now + FMath::FRandRange(CalmInterval.X, CalmInterval.Y) * Scale;
    }

    if (Now >= NextStingerCheck)
    {
        NextStingerCheck = Now + 0.25f;
        CheckStingers(Eye, Pawn->GetViewRotation().Vector());
    }
}

void UAHAmbienceComponent::PlayDistantEvent(const FVector& Listener)
{
    float Total = 0.f;
    for (const FDistantSound& S : DistantSounds)
    {
        Total += S.Weight;
    }
    float Pick = FMath::FRandRange(0.f, Total);
    const FDistantSound* Chosen = &DistantSounds[0];
    for (const FDistantSound& S : DistantSounds)
    {
        Pick -= S.Weight;
        if (Pick <= 0.f)
        {
            Chosen = &S;
            break;
        }
    }
    USoundBase* Sound = AHRealism::RandomVariant(Chosen->Prefix, Chosen->Variants);
    if (!Sound)
    {
        return;
    }
    const float Angle = FMath::FRandRange(0.f, 2.f * UE_PI);
    const float Distance = FMath::FRandRange(EventDistance.X, EventDistance.Y);
    const FVector Where = Listener + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance,
        FMath::FRandRange(Chosen->HeightMin, Chosen->HeightMax) - 100.f);
    AHRealism::PlayAt(this, Sound, Where, Chosen->Volume, FMath::FRandRange(0.85f, 1.1f), 6000.f);
}

void UAHAmbienceComponent::CheckStingers(const FVector& Eye, const FVector& Forward)
{
    UWorld* W = GetWorld();
    const float Now = static_cast<float>(W->GetTimeSeconds());
    if (Now >= NextStingerRefresh)
    {
        NextStingerRefresh = Now + 5.f;
        StingerActors.Reset();
        for (TActorIterator<AActor> It(W); It; ++It)
        {
            if (*It && It->ActorHasTag(TagStinger))
            {
                StingerActors.Add(*It);
            }
        }
    }
    if (Now - LastStingTime < StingerCooldown)
    {
        return;
    }
    for (const TWeakObjectPtr<AActor>& Weak : StingerActors)
    {
        AActor* Thing = Weak.Get();
        if (!Thing || Stung.Contains(Weak))
        {
            continue;
        }
        const FVector Target = Thing->GetActorLocation() + FVector(0.f, 0.f, 40.f);
        const FVector To = Target - Eye;
        const float Dist = static_cast<float>(To.Size());
        if (Dist > StingerRange || Dist < 1.f || FVector::DotProduct(Forward, To / Dist) < 0.8f)
        {
            continue;
        }
        FCollisionQueryParams Params(SCENE_QUERY_STAT(AHStinger), false, GetOwner());
        Params.AddIgnoredActor(Thing);
        FHitResult Hit;
        if (W->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params))
        {
            continue;   // a wall in the way
        }
        Stung.Add(Weak);
        LastStingTime = Now;
        if (USoundBase* Sting = AHRealism::RandomVariant(TEXT("SW_Stinger"), 2))
        {
            UGameplayStatics::PlaySound2D(this, Sting, 0.85f);
        }
        if (const AMyFPSPawn* Pawn = Cast<AMyFPSPawn>(GetOwner()))
        {
            if (UBodycamLensComponent* Lens = Pawn->GetBodycamLens())
            {
                Lens->ForceGlitch(0.3f, false);
            }
        }
        break;
    }
}
