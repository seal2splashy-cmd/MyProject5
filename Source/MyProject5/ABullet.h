// ABullet.h
// Physical projectile: manual gravity + quadratic drag, sphere-swept every tick so
// fast bullets can't tunnel through thin walls.
// [Rebuilt Oct 2026 from coursework excerpts - same design and tuning as the original]
// Nov 2026 realism pass: goes through thin wood / sheet metal / glass / leaves (losing energy),
// glances off concrete and metal at shallow angles, per-surface impact sounds, impacts are
// noises enemies hear, hit zones (head x2.5, legs x0.7). Docs/REALISM.md

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABullet.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

UCLASS()
class MYPROJECT5_API ABullet : public AActor
{
    GENERATED_BODY()

public:
    ABullet();

    /** Called by AGun straight after spawning. */
    void Init(const FVector& InitialVelocity, float InDamage);

    virtual void Tick(float DeltaTime) override;

private:
    void DoCollisionSweep(const FVector& Start, const FVector& End);
    /** True if the bullet carries on (went through or glanced off). */
    bool OnImpact(const FHitResult& Hit);
    /** Damage multiplier for where on the body it hit. */
    float HitZoneMultiplier(const FHitResult& Hit) const;
    void ImpactEffects(const FHitResult& Hit, bool bRicochet) const;

    UPROPERTY(VisibleAnywhere)
    UStaticMeshComponent* MeshComp;

    UPROPERTY(EditAnywhere, Category = "Bullet|Physics")
    float GravityZ = -980.f;

    /** Quadratic drag constant. 0.001 slowed bullets far too fast; 0.00004 was the tested value. */
    UPROPERTY(EditAnywhere, Category = "Bullet|Physics")
    float DragK = 0.00004f;

    UPROPERTY(EditAnywhere, Category = "Bullet|Physics")
    float CollisionRadius = 1.5f;

    /** Seconds before an unimpacted bullet destroys itself. */
    UPROPERTY(EditAnywhere, Category = "Bullet|Physics")
    float MaxLife = 6.f;

    UPROPERTY(EditAnywhere, Category = "Bullet|Impact")
    float ImpulseScale = 40.f;

    /** How many things it can go through or bounce off before it stops. */
    UPROPERTY(EditAnywhere, Category = "Bullet|Impact")
    int32 MaxPenetrations = 2;

    UPROPERTY(EditAnywhere, Category = "Bullet|Impact")
    float HeadshotMultiplier = 2.5f;

    UPROPERTY(EditAnywhere, Category = "Bullet|Impact")
    float LegMultiplier = 0.7f;

    /** Below this speed it's spent and just drops (cm/s). */
    UPROPERTY(EditAnywhere, Category = "Bullet|Impact")
    float MinSpeed = 4000.f;

    FVector Velocity = FVector::ZeroVector;
    float Damage = 0.f;
    float Life = 0.f;
    bool bInitialised = false;
    int32 Penetrations = 0;
    float InitialSpeed = 1.f;

    /** What it last went through, so the next sweep doesn't hit its far side again. */
    TWeakObjectPtr<UPrimitiveComponent> LastPenetrated;
};
