// RealismUtils.cpp

#include "RealismUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/Package.h"

namespace AHRealism
{
    // Name, pen cm, ricochet deg, impact noise cm, impact sound, n, footstep sound, n, footstep loudness
    static const FSurfaceInfo GSurfaces[] =
    {
        { TEXT("Concrete"), 0.f,   12.f, 1500.f, TEXT("SW_Impact_Concrete"), 3, TEXT("SW_Footstep_Concrete"), 4, 1.0f  },
        { TEXT("Metal"),    0.8f,  18.f, 2200.f, TEXT("SW_Impact_Metal"),    3, TEXT("SW_Footstep_Metal"),    4, 1.5f  },
        { TEXT("Wood"),     25.f,  0.f,  1200.f, TEXT("SW_Impact_Wood"),     3, TEXT("SW_Footstep_Wood"),     3, 1.1f  },
        { TEXT("Dirt"),     0.f,   0.f,  700.f,  TEXT("SW_Impact_Dirt"),     2, TEXT("SW_Footstep_Dirt"),     3, 0.6f  },
        { TEXT("Grass"),    0.f,   0.f,  500.f,  TEXT("SW_Impact_Dirt"),     2, TEXT("SW_Footstep_Grass"),    3, 0.45f },
        { TEXT("Foliage"),  400.f, 0.f,  250.f,  nullptr,                    0, TEXT("SW_Footstep_Grass"),    3, 0.45f },
        { TEXT("Glass"),    3.f,   0.f,  1800.f, TEXT("SW_Impact_Glass"),    2, TEXT("SW_Footstep_Concrete"), 4, 1.0f  },
        { TEXT("Flesh"),    0.f,   0.f,  600.f,  TEXT("SW_Impact_Flesh"),    2, TEXT("SW_Footstep_Concrete"), 4, 1.0f  },
    };

    const FSurfaceInfo& Info(ESurface Surface)
    {
        const int32 Index = FMath::Clamp(static_cast<int32>(Surface), 0, static_cast<int32>(UE_ARRAY_COUNT(GSurfaces)) - 1);
        return GSurfaces[Index];
    }

    static bool HasAny(const FString& Key, std::initializer_list<const TCHAR*> Words)
    {
        for (const TCHAR* Word : Words)
        {
            if (Key.Contains(Word))
            {
                return true;
            }
        }
        return false;
    }

    ESurface SurfaceFromHit(const FHitResult& Hit)
    {
        const AActor* HitActor = Hit.GetActor();
        if (HitActor && HitActor->IsA<APawn>())
        {
            return ESurface::Flesh;
        }

        // There are no physical-material surface types set up in this project, so read it off the
        // names: the material (M_FR_Steel, M_RustMetal_World, M_FR_Bark ...), the mesh
        // (SM_FR_Generator ...) and the actor label (FR_Tree_03_CanopyLow ...).
        FString Key;
        if (const UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
        {
            Key += PhysMat->GetName();
        }
        if (const UPrimitiveComponent* Comp = Hit.GetComponent())
        {
            if (const UMaterialInterface* Mat = Comp->GetMaterial(0))
            {
                Key += TEXT(" ") + Mat->GetName();
            }
            if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
            {
                if (const UStaticMesh* Mesh = SMC->GetStaticMesh())
                {
                    Key += TEXT(" ") + Mesh->GetName();
                }
            }
        }
        if (HitActor)
        {
            Key += TEXT(" ") + HitActor->GetActorNameOrLabel();
        }
        Key = Key.ToLower();

        // Order matters: "outdoor" contains "door", "storagecrate" is a metal ammo can, ...
        if (HasAny(Key, { TEXT("canopy"), TEXT("pine"), TEXT("foliage"), TEXT("leaf"), TEXT("leaves"), TEXT("bush"), TEXT("hedge") }))
            return ESurface::Foliage;
        if (HasAny(Key, { TEXT("glass"), TEXT("window"), TEXT("display"), TEXT("screen"), TEXT("lightpanel") }))
            return ESurface::Glass;
        if (HasAny(Key, { TEXT("soil"), TEXT("dirt"), TEXT("mud"), TEXT("ground"), TEXT("gravel") }))
            return ESurface::Dirt;
        if (HasAny(Key, { TEXT("grass"), TEXT("moss"), TEXT("lawn") }))
            return ESurface::Grass;
        if (HasAny(Key, { TEXT("metal"), TEXT("steel"), TEXT("rust"), TEXT("oxide"), TEXT("iron"), TEXT("grate"),
                          TEXT("pipe"), TEXT("generator"), TEXT("shelf"), TEXT("shelving"), TEXT("door"), TEXT("duct"),
                          TEXT("barrel"), TEXT("locker"), TEXT("rack"), TEXT("cabinet"), TEXT("brace"), TEXT("tray"),
                          TEXT("fence"), TEXT("storagecrate"), TEXT("crateband"), TEXT("safetyyellow"), TEXT("sign"),
                          TEXT("fixture"), TEXT("hanginglight") }))
            return ESurface::Metal;
        if (HasAny(Key, { TEXT("wood"), TEXT("bark"), TEXT("trunk"), TEXT("plank"), TEXT("pallet"), TEXT("crate") }))
            return ESurface::Wood;
        return ESurface::Concrete;
    }

    ESurface SurfaceUnder(UWorld* World, const FVector& Location, float Depth, const AActor* Ignore)
    {
        if (!World)
        {
            return ESurface::Concrete;
        }
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(AHSurfaceUnder), false, Ignore);
        Params.bReturnPhysicalMaterial = true;
        if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 5.f), Location - FVector(0.f, 0.f, Depth),
                ECC_Visibility, Params))
        {
            return SurfaceFromHit(Hit);
        }
        return ESurface::Concrete;
    }

    USoundBase* GeneratedSound(const FString& Name)
    {
        // DoesPackageExist hits the disk, so remember the answer per name.
        static TMap<FString, bool> Exists;
        const FString Package = TEXT("/Game/Generated/Audio/") + Name;
        bool* Known = Exists.Find(Name);
        if (!Known)
        {
            Known = &Exists.Add(Name, FPackageName::DoesPackageExist(Package));
        }
        if (!*Known)
        {
            return nullptr;
        }
        return Cast<USoundBase>(FSoftObjectPath(Package + TEXT(".") + Name).TryLoad());
    }

    USoundBase* RandomVariant(const TCHAR* Prefix, int32 Count)
    {
        if (!Prefix || Count <= 0)
        {
            return nullptr;
        }
        const int32 Start = FMath::RandRange(1, Count);
        for (int32 i = 0; i < Count; ++i)   // a missing variant falls through to the next one
        {
            const int32 V = (Start - 1 + i) % Count + 1;
            if (USoundBase* Sound = GeneratedSound(FString::Printf(TEXT("%s_%02d"), Prefix, V)))
            {
                return Sound;
            }
        }
        return nullptr;
    }

    USoundAttenuation* Attenuation(float AudibleRange)
    {
        // A handful of buckets is plenty; keep them alive for the whole session.
        // (AddToRoot keeps them from being garbage collected, so plain pointers are fine)
        static TMap<int32, USoundAttenuation*> Cache;
        const int32 Bucket = FMath::Clamp(FMath::RoundToInt(AudibleRange / 250.f), 1, 400);
        if (USoundAttenuation** Found = Cache.Find(Bucket))
        {
            return *Found;
        }

        const float Range = Bucket * 250.f;
        USoundAttenuation* Att = NewObject<USoundAttenuation>(GetTransientPackage());
        Att->AddToRoot();
        FSoundAttenuationSettings& S = Att->Attenuation;
        S.bAttenuate = true;
        S.bSpatialize = true;
        S.AttenuationShape = EAttenuationShape::Sphere;
        S.AttenuationShapeExtents = FVector(Range * 0.08f, 0.f, 0.f);   // full volume this close
        S.FalloffDistance = Range * 0.92f;
        S.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;  // like air: -60 dB at the edge
        // air soaks up the highs over distance
        S.bAttenuateWithLPF = true;
        S.LPFRadiusMin = Range * 0.15f;
        S.LPFRadiusMax = Range;
        S.LPFFrequencyAtMin = 20000.f;
        S.LPFFrequencyAtMax = 1800.f;
        // through a wall: quieter and muffled
        S.bEnableOcclusion = true;
        S.OcclusionTraceChannel = ECC_Visibility;
        S.OcclusionLowPassFilterFrequency = 900.f;
        S.OcclusionVolumeAttenuation = 0.35f;
        S.OcclusionInterpolationTime = 0.1f;

        Cache.Add(Bucket, Att);
        return Att;
    }

    void PlayAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Location,
        float Volume, float Pitch, float AudibleRange)
    {
        if (!Sound || !WorldContext)
        {
            return;
        }
        UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator,
            Volume, Pitch, 0.f, Attenuation(AudibleRange));
    }
}
