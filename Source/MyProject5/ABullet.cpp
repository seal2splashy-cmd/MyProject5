// ABullet.cpp
// [Rebuilt Oct 2026 from coursework excerpts; penetration / ricochet / surfaces added Nov 2026]

#include "ABullet.h"
#include "AEnemy.h"
#include "RealismUtils.h"
#include "Components/CapsuleComponent.h"
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
    Penetrations = 0;
    InitialSpeed = FMath::Max(1.f, static_cast<float>(InitialVelocity.Size()));
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
    Params.bReturnPhysicalMaterial = true;
    if (AActor* OwnerActor = GetOwner()) Params.AddIgnoredActor(OwnerActor);
    if (APawn* IP = GetInstigator()) Params.AddIgnoredActor(IP);
    if (UPrimitiveComponent* Prev = LastPenetrated.Get()) Params.AddIgnoredComponent(Prev);

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
        if (!OnImpact(Hit))
        {
            Destroy();
        }
    }
    else
    {
        SetActorLocation(End);
        LastPenetrated.Reset();
    }
}

float ABullet::HitZoneMultiplier(const FHitResult& Hit) const
{
    // Skeletal mesh with a physics asset: go by bone
    const FString Bone = Hit.BoneName.ToString().ToLower();
    if (!Bone.IsEmpty() && Bone != TEXT("none"))
    {
        if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck"))) return HeadshotMultiplier;
        if (Bone.Contains(TEXT("thigh")) || Bone.Contains(TEXT("calf")) || Bone.Contains(TEXT("foot")) ||
            Bone.Contains(TEXT("leg"))) return LegMultiplier;
        return 1.f;
    }
    // Capsule (the Gaunt, enemies without a physics asset): go by height up the capsule
    if (const UCapsuleComponent* Cap = Cast<UCapsuleComponent>(Hit.GetComponent()))
    {
        const float HalfHeight = Cap->GetScaledCapsuleHalfHeight();
        const float Bottom = Cap->GetComponentLocation().Z - HalfHeight;
        const float Up01 = (Hit.ImpactPoint.Z - Bottom) / FMath::Max(2.f * HalfHeight, 1.f);
        if (Up01 >= 0.86f) return HeadshotMultiplier;
        if (Up01 <= 0.42f) return LegMultiplier;
    }
    return 1.f;
}

void ABullet::ImpactEffects(const FHitResult& Hit, bool bRicochet) const
{
    const AHRealism::ESurface Surface = AHRealism::SurfaceFromHit(Hit);
    const AHRealism::FSurfaceInfo& Info = AHRealism::Info(Surface);
    const float Energy = FMath::Clamp(static_cast<float>(Velocity.Size()) / InitialSpeed, 0.2f, 1.f);

    if (USoundBase* Sound = AHRealism::RandomVariant(Info.ImpactSound, Info.ImpactVariants))
    {
        AHRealism::PlayAt(this, Sound, Hit.ImpactPoint, 0.9f * Energy, FMath::FRandRange(0.9f, 1.1f), 3500.f);
    }
    if (bRicochet)
    {
        AHRealism::PlayAt(this, AHRealism::RandomVariant(TEXT("SW_Ricochet"), 2), Hit.ImpactPoint, 0.8f,
            FMath::FRandRange(0.9f, 1.15f), 4000.f);
    }

    // Enemies near where it landed hear it and come to look - it can draw them away from you.
    // (No noise maker: this sound is where the bullet landed, not where the shooter is.)
    AEnemy::ReportNoise(this, Hit.ImpactPoint, Info.ImpactNoiseRange * Energy, nullptr);
}

bool ABullet::OnImpact(const FHitResult& Hit)
{
    AActor* HitActor = Hit.GetActor();
    const FVector Dir = Velocity.GetSafeNormal();
    const AHRealism::ESurface Surface = AHRealism::SurfaceFromHit(Hit);
    const AHRealism::FSurfaceInfo& Info = AHRealism::Info(Surface);

    if (HitActor && HitActor != this && HitActor->CanBeDamaged())
    {
        AController* InstigatorController = nullptr;
        if (APawn* IP = GetInstigator())
        {
            InstigatorController = IP->GetController();
        }

        // Spent bullets hurt less
        const float Energy = FMath::Clamp(static_cast<float>(Velocity.Size()) / InitialSpeed, 0.15f, 1.f);
        const float Applied = Damage * Energy * (Surface == AHRealism::ESurface::Flesh ? HitZoneMultiplier(Hit) : 1.f);
        UGameplayStatics::ApplyPointDamage(HitActor, Applied, Dir, Hit,
            InstigatorController, this, UDamageType::StaticClass());
    }

    // Shove physics props / ragdolls, proportional to damage
    UPrimitiveComponent* Comp = Hit.GetComponent();
    if (Comp && Comp->IsSimulatingPhysics(Hit.BoneName))
    {
        Comp->AddImpulseAtLocation(Dir * Damage * ImpulseScale, Hit.ImpactPoint, Hit.BoneName);
    }

    if (Surface == AHRealism::ESurface::Flesh || !Comp || Penetrations >= MaxPenetrations)
    {
        ImpactEffects(Hit, false);
        return false;   // stops in the body / out of energy for tricks
    }

    // ---- Glance off? Shallow hits on hard surfaces can skip off instead of digging in.
    const float GrazeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(
        static_cast<float>(FMath::Abs(FVector::DotProduct(Dir, Hit.ImpactNormal))), 0.f, 1.f)));
    if (Info.RicochetMaxAngleDeg > 0.f && GrazeDeg < Info.RicochetMaxAngleDeg &&
        FMath::FRand() < 1.f - GrazeDeg / Info.RicochetMaxAngleDeg)
    {
        ImpactEffects(Hit, true);
        FVector NewDir = FMath::GetReflectionVector(Dir, Hit.ImpactNormal);
        NewDir = FMath::VRandCone(NewDir, FMath::DegreesToRadians(6.f));
        Velocity = NewDir * Velocity.Size() * 0.45f;
        Damage *= 0.5f;
        ++Penetrations;
        LastPenetrated = Comp;
        SetActorLocation(Hit.ImpactPoint + Hit.ImpactNormal * (CollisionRadius + 0.5f));
        return Velocity.Size() > MinSpeed;
    }

    // ---- Go through? Find the far side: trace back from deeper in towards the entry point.
    if (Info.PenetrationCm > 0.f)
    {
        const float Probe = Info.PenetrationCm + 1.f;
        const FVector Inside = Hit.ImpactPoint + Dir * Probe;
        FHitResult Exit;
        FCollisionQueryParams ExitParams(SCENE_QUERY_STAT(BulletExit), true);
        const bool bExit = Comp->LineTraceComponent(Exit, Inside, Hit.ImpactPoint, ExitParams);
        if (bExit && !Exit.bStartPenetrating && Exit.Distance > KINDA_SMALL_NUMBER)
        {
            const float Thickness = FVector::Dist(Hit.ImpactPoint, Exit.ImpactPoint);
            const float Left = 1.f - Thickness / Info.PenetrationCm;
            if (Left > 0.05f)
            {
                ImpactEffects(Hit, false);
                Velocity = FMath::VRandCone(Dir, FMath::DegreesToRadians(2.f)) * Velocity.Size() * FMath::Lerp(0.35f, 0.9f, Left);
                Damage *= FMath::Lerp(0.4f, 0.9f, Left);
                ++Penetrations;
                LastPenetrated = Comp;
                SetActorLocation(Exit.ImpactPoint + Dir * (CollisionRadius + 0.5f));
                return Velocity.Size() > MinSpeed;
            }
        }
    }

    ImpactEffects(Hit, false);
    return false;
}
