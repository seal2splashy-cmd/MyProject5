// RealismUtils.h
// Shared bits for the realism pass:
//  - what a surface is made of (from the hit's material / mesh / actor name), and how a
//    9 mm round, a footstep and a bullet impact behave on it
//  - generated sounds by name (silently nullptr if not imported)
//  - 3D sound with real-world falloff: quieter and duller with distance, muffled through walls

#pragma once

#include "CoreMinimal.h"

class USoundBase;
class USoundAttenuation;
class UWorld;
class AActor;
struct FHitResult;

namespace AHRealism
{
    enum class ESurface : uint8
    {
        Concrete,   // default for anything unrecognised
        Metal,
        Wood,
        Dirt,
        Grass,
        Foliage,    // leaves / canopies: bullets pass through
        Glass,
        Flesh,
    };

    struct FSurfaceInfo
    {
        const TCHAR* Name;
        /** Solid thickness a 9 mm pistol round gets through (cm). 0 = stops it. */
        float PenetrationCm;
        /** Glancing hits flatter than this (degrees off the surface) can ricochet. 0 = never. */
        float RicochetMaxAngleDeg;
        /** How far enemies hear a bullet hit it (cm). */
        float ImpactNoiseRange;
        const TCHAR* ImpactSound;   // SW_<this>_01.. ; nullptr = none
        int32 ImpactVariants;
        const TCHAR* FootstepSound; // SW_<this>_01..
        int32 FootstepVariants;
        /** Footstep loudness (x the normal walking noise): metal clangs, grass is soft. */
        float FootstepLoudness;
    };

    /** What the thing in Hit is made of. */
    MYPROJECT5_API ESurface SurfaceFromHit(const FHitResult& Hit);

    /** What's under Location (a short trace straight down). */
    MYPROJECT5_API ESurface SurfaceUnder(UWorld* World, const FVector& Location, float Depth, const AActor* Ignore);

    MYPROJECT5_API const FSurfaceInfo& Info(ESurface Surface);

    /** /Game/Generated/Audio/<Name>, or nullptr if it isn't imported (checked once, then cached). */
    MYPROJECT5_API USoundBase* GeneratedSound(const FString& Name);

    /** A random one of <Prefix>_01 .. <Prefix>_<Count> that exists, or nullptr. */
    MYPROJECT5_API USoundBase* RandomVariant(const TCHAR* Prefix, int32 Count);

    /** Falloff that sounds like the real world: full volume within ~1/10 of Range, quieter and
     *  duller (low-pass) towards Range, muffled when something solid is in between. Cached per range. */
    MYPROJECT5_API USoundAttenuation* Attenuation(float AudibleRange);

    /** PlaySoundAtLocation with Attenuation(AudibleRange). Does nothing if Sound is null. */
    MYPROJECT5_API void PlayAt(const UObject* WorldContext, USoundBase* Sound, const FVector& Location,
        float Volume = 1.f, float Pitch = 1.f, float AudibleRange = 3000.f);
}
