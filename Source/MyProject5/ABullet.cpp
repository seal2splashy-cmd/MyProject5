// ABullet.cpp
// [Rebuilt Oct 2026 from coursework excerpts]

#include "ABullet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ABullet::ABullet()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // we sweep manually
    MeshComp->CastShadow = false;

    // Engine sphere always exists, so this can't fail on a fresh project
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        MeshComp->SetStaticMesh(SphereMesh.Object);
    }
    MeshComp->SetRelativeScale3D(FVector(0.03f));
}

void ABullet::Init(const FVector& InitialVelocity, float InDamage)
{
    Velocity = InitialVelocity;
    Damage = InDamage;
    Life = 0.f;
    bInitialised = true;
}

void ABullet::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bInitialised) return;

    Life += DeltaTime;
    if (Life >= MaxLife)
    {
        Destroy();
        return;
    }

    // Gravity
    Velocity.Z += GravityZ * DeltaTime;

    // Quadratic drag: decelerates proportional to speed squared
    const float Speed = Velocity.Size();
    if (Speed > KINDA_SMALL_NUMBER)
    {
        const FVector Drag = -Velocity * Speed * DragK;
        Velocity += Drag * DeltaTime;
    }

    const FVector Start = GetActorLocation();
    const FVector End = Start + Velocity * DeltaTime;
    DoCollisionSweep(Start, End);
}

void ABullet::DoCollisionSweep(const FVector& Start, const FVector& End)
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BulletSweep), true, this);
    if (AActor* OwnerActor = GetOwner()) Params.AddIgnoredActor(OwnerActor);
    if (APawn* IP = GetInstigator()) Params.AddIgnoredActor(IP);

    FCollisionObjectQueryParams ObjQuery;
    ObjQuery.AddObjectTypesToQuery(ECC_Pawn);
    ObjQuery.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

    const bool bHit = GetWorld()->SweepSingleByObjectType(
        Hit, Start, End, FQuat::Identity,
        ObjQuery, FCollisionShape::MakeSphere(CollisionRadius), Params);

    if (bHit && Hit.bBlockingHit)
    {
        SetActorLocation(Hit.Location);
        OnImpact(Hit);
    }
    else
    {
        SetActorLocation(End);
    }
}

void ABullet::OnImpact(const FHitResult& Hit)
{
    AActor* HitActor = Hit.GetActor();

    if (HitActor && HitActor != this)
    {
        AController* InstigatorController = nullptr;
        if (APawn* IP = GetInstigator())
        {
            InstigatorController = IP->GetController();
        }

        UGameplayStatics::ApplyPointDamage(HitActor, Damage, Velocity.GetSafeNormal(), Hit,
            InstigatorController, this, UDamageType::StaticClass());
    }

    // Shove physics props / ragdolls, proportional to damage
    if (UPrimitiveComponent* Comp = Hit.GetComponent())
    {
        if (Comp->IsSimulatingPhysics(Hit.BoneName))
        {
            Comp->AddImpulseAtLocation(Velocity.GetSafeNormal() * Damage * ImpulseScale, Hit.ImpactPoint, Hit.BoneName);
        }
    }

    Destroy();
}
