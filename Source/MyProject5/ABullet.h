// ABullet.h
// Physical projectile: manual gravity + quadratic drag, sphere-swept every tick so
// fast bullets can't tunnel through thin walls.
// [Rebuilt Oct 2026 from coursework excerpts - same design and tuning as the original]

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABullet.generated.h"

class UStaticMeshComponent;

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
    void OnImpact(const FHitResult& Hit);

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

    FVector Velocity = FVector::ZeroVector;
    float Damage = 0.f;
    float Life = 0.f;
    bool bInitialised = false;
};
