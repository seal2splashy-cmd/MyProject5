// MeleeWeapon.cpp

#include "MeleeWeapon.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AMeleeWeapon::AMeleeWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after the pawn has moved/rotated the camera

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Grip"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // we do our own sweeps
	Mesh->SetGenerateOverlapEvents(false);

	BladeBase = CreateDefaultSubobject<USceneComponent>(TEXT("BladeBase"));
	BladeBase->SetupAttachment(Mesh);
	BladeBase->SetRelativeLocation(FVector(15.f, 0.f, 0.f));

	BladeTip = CreateDefaultSubobject<USceneComponent>(TEXT("BladeTip"));
	BladeTip->SetupAttachment(Mesh);
	BladeTip->SetRelativeLocation(FVector(90.f, 0.f, 0.f));
}

void AMeleeWeapon::BeginPlay()
{
	Super::BeginPlay();
	CacheBladeLocals();
}

void AMeleeWeapon::CacheBladeLocals()
{
	// Blade points relative to the grip, so we can evaluate them for any
	// sub-step pose without actually moving the actor.
	const FTransform MeshRel = Mesh->GetRelativeTransform();
	LocalBase = MeshRel.TransformPosition(BladeBase->GetRelativeLocation());
	LocalTip = MeshRel.TransformPosition(BladeTip->GetRelativeLocation());
}

void AMeleeWeapon::InitialiseFor(UCameraComponent* InCamera)
{
	Camera = InCamera;
	if (!InCamera)
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		AddTickPrerequisiteActor(OwnerActor);
	}

	CacheBladeLocals();
	SnapToTarget(InCamera->GetComponentTransform());
}

void AMeleeWeapon::SnapToTarget(const FTransform& CamXf)
{
	LastCameraPos = CamXf.GetLocation();
	HandPos = ComputeTargetWorld(CamXf);
	HandVel = FVector::ZeroVector;
	HandRot = ComputeTargetRotation(CamXf, HandPos, FVector::ZeroVector);
	GatherBladePoints(HandPos, HandRot, PrevBladePoints);
	HitThisSwing.Reset();
	TipSpeed = 0.f;
	SetActorLocationAndRotation(HandPos, HandRot);
}

void AMeleeWeapon::AddMouseInput(const FVector2D& LookDelta)
{
	if (!bGuarding)
	{
		return; // not guarding: the mouse only turns the camera, sway comes from inertia
	}

	FVector2D D = LookDelta * SwingSensitivity;
	if (bInvertSwingY)
	{
		D.Y = -D.Y;
	}

	GuardOffset += D;
	if (GuardOffset.Size() > ControlRadius)
	{
		GuardOffset = GuardOffset.GetSafeNormal() * ControlRadius;
	}
}

void AMeleeWeapon::BeginGuard()
{
	bGuarding = true;
	GuardOffset = FVector2D::ZeroVector;
}

void AMeleeWeapon::EndGuard()
{
	bGuarding = false;
	GuardOffset = FVector2D::ZeroVector;
}

void AMeleeWeapon::Thrust()
{
	if (const UCameraComponent* Cam = Camera.Get())
	{
		HandVel += Cam->GetForwardVector() * (ThrustImpulse / MassKg);
	}
}

FVector AMeleeWeapon::ComputeTargetWorld(const FTransform& CamXf) const
{
	FVector Local = RestOffset;
	if (bGuarding)
	{
		Local += GuardPose + FVector(0.f, GuardOffset.X, GuardOffset.Y);
	}
	return CamXf.TransformPosition(Local);
}

FQuat AMeleeWeapon::ComputeTargetRotation(const FTransform& CamXf, const FVector& Hand, const FVector& RelVel) const
{
	const FVector CamFwd = CamXf.GetUnitAxis(EAxis::X);
	const FVector CamUp = CamXf.GetUnitAxis(EAxis::Z);
	const FVector Shoulder = CamXf.TransformPosition(ShoulderOffset);

	// Blade leans out along the shoulder->hand line (pendulum), tilted up so it reads as "held".
	const FVector Out = (Hand - Shoulder).GetSafeNormal(UE_SMALL_NUMBER, CamFwd);
	const FVector BladeDir = (Out + CamUp * BladeUpBias).GetSafeNormal(UE_SMALL_NUMBER, CamFwd);

	// Edge faces forward at rest, and turns into the direction of travel when you actually swing.
	const float W = FMath::Clamp(RelVel.Size() / EdgeAlignSpeed, 0.f, 1.f);
	FVector EdgeHint = FMath::Lerp(CamFwd, RelVel.GetSafeNormal(), W);
	EdgeHint -= FVector::DotProduct(EdgeHint, BladeDir) * BladeDir;
	if (EdgeHint.SizeSquared() < 1e-4f)
	{
		EdgeHint = CamUp - FVector::DotProduct(CamUp, BladeDir) * BladeDir;
	}

	return FRotationMatrix::MakeFromXZ(BladeDir, EdgeHint.GetSafeNormal()).ToQuat();
}

void AMeleeWeapon::GatherBladePoints(const FVector& Pos, const FQuat& Rot, TArray<FVector>& Out) const
{
	const FTransform Xf(Rot, Pos, GetActorScale3D());
	const FVector A = Xf.TransformPosition(LocalBase);
	const FVector B = Xf.TransformPosition(LocalTip);

	const int32 N = FMath::Max(2, BladeSamples);
	Out.Reset(N);
	for (int32 i = 0; i < N; ++i)
	{
		Out.Add(FMath::Lerp(A, B, static_cast<float>(i) / static_cast<float>(N - 1)));
	}
}

void AMeleeWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UCameraComponent* Cam = Camera.Get();
	if (!Cam || DeltaSeconds <= 0.f)
	{
		return;
	}

	const FTransform CamXf = Cam->GetComponentTransform();
	const FVector CamPos = CamXf.GetLocation();

	if (FVector::DistSquared(CamPos, LastCameraPos) > FMath::Square(TeleportSnapDistance))
	{
		SnapToTarget(CamXf);
		return;
	}

	const float Dt = FMath::Min(DeltaSeconds, 0.1f); // don't explode on hitches
	const FVector CamVel = (CamPos - LastCameraPos) / Dt;
	LastCameraPos = CamPos;

	const int32 Steps = FMath::Max(1, SubSteps);
	const float H = Dt / static_cast<float>(Steps);

	const FVector Target = ComputeTargetWorld(CamXf);
	const float C = 2.f * DampingRatio * FMath::Sqrt(Stiffness * MassKg);

	if (PrevBladePoints.Num() == 0)
	{
		GatherBladePoints(HandPos, HandRot, PrevBladePoints);
	}
	const FVector FrameStartTip = PrevBladePoints.Last();

	TArray<FVector> CurrPoints;
	CurrPoints.Reserve(PrevBladePoints.Num());

	for (int32 Step = 0; Step < Steps; ++Step)
	{
		// Spring-damper towards the target. Damping is relative to the camera's velocity,
		// so walking doesn't make the weapon trail miles behind — only turning and swinging do.
		const FVector Force = Stiffness * (Target - HandPos) - C * (HandVel - CamVel);
		HandVel += (Force / MassKg) * H;   // semi-implicit Euler: velocity first...
		HandPos += HandVel * H;            // ...then position. Stable at these step sizes.

		const FQuat TargetRot = ComputeTargetRotation(CamXf, HandPos, HandVel - CamVel);
		HandRot = FQuat::Slerp(HandRot, TargetRot, 1.f - FMath::Exp(-RotationSharpness * H));

		GatherBladePoints(HandPos, HandRot, CurrPoints);

		if (SweepBlade(PrevBladePoints, CurrPoints, H))
		{
			// The hit changed our position/velocity; re-sample so next step sweeps from the right place.
			GatherBladePoints(HandPos, HandRot, CurrPoints);
		}

		if (bDrawDebug)
		{
			const float Speed01 = FMath::Clamp(HandVel.Size() / 1500.f, 0.f, 1.f);
			const FColor Col = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Speed01).ToFColor(true);
			DrawDebugLine(GetWorld(), CurrPoints[0], CurrPoints.Last(), Col, false, 0.4f, 0, 0.6f);
		}

		PrevBladePoints = CurrPoints;
	}

	TipSpeed = FVector::Dist(FrameStartTip, PrevBladePoints.Last()) / Dt;
	if (TipSpeed < SwingResetSpeed)
	{
		HitThisSwing.Reset();
	}

	SetActorLocationAndRotation(HandPos, HandRot);
}

bool AMeleeWeapon::SweepBlade(const TArray<FVector>& From, const TArray<FVector>& To, float StepDt)
{
	UWorld* World = GetWorld();
	if (!World || From.Num() != To.Num() || StepDt <= 0.f)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeSweep), /*bTraceComplex*/ false, this);
	Params.AddIgnoredActor(GetOwner());
	Params.bReturnPhysicalMaterial = true; // so you can do metal vs wood vs flesh sounds later
	for (const TWeakObjectPtr<AActor>& Already : HitThisSwing)
	{
		if (Already.IsValid())
		{
			Params.AddIgnoredActor(Already.Get()); // one bite per enemy per swing
		}
	}

	const FCollisionShape Sphere = FCollisionShape::MakeSphere(TraceRadius);

	// Tip first: it moves fastest, so it's the most likely to be the "real" hit.
	for (int32 i = To.Num() - 1; i >= 0; --i)
	{
		if (From[i].Equals(To[i], 0.01f))
		{
			continue;
		}

		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, From[i], To[i], FQuat::Identity, TraceChannel, Sphere, Params))
		{
			HandleHit(Hit, (To[i] - From[i]) / StepDt);
			return true;
		}
	}
	return false;
}

void AMeleeWeapon::HandleHit(const FHitResult& Hit, const FVector& PointVel)
{
	// Blade started inside something (resting against a wall etc.) — just push out, no damage.
	if (Hit.bStartPenetrating)
	{
		HandPos += Hit.Normal * (Hit.PenetrationDepth + 0.5f);
		const float Into = FVector::DotProduct(HandVel, Hit.Normal);
		if (Into < 0.f)
		{
			HandVel -= Into * Hit.Normal;
		}
		return;
	}

	const FVector N = Hit.ImpactNormal;

	// Only the component of velocity going INTO the surface counts. Glancing blows do little.
	const float ImpactSpeed = FMath::Max(0.f, -FVector::DotProduct(PointVel, N));

	AActor* HitActor = Hit.GetActor();
	const bool bFlesh = HitActor && HitActor->CanBeDamaged() && HitActor->IsA<APawn>();

	if (bFlesh)
	{
		HitThisSwing.Add(HitActor);

		if (ImpactSpeed >= MinDamageSpeed)
		{
			AController* InstigatorController = nullptr;
			if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
			{
				InstigatorController = OwnerPawn->GetController();
			}

			const float Damage = (ImpactSpeed - MinDamageSpeed) * MassKg * DamagePerMomentum;
			UGameplayStatics::ApplyPointDamage(HitActor, Damage, PointVel.GetSafeNormal(), Hit,
				InstigatorController, this, UDamageType::StaticClass());
		}

		// The blade bites and drags rather than bouncing.
		HandVel *= FleshVelocityKeep;
	}
	else
	{
		// Hard surface: reflect the into-surface part of the velocity. This is the "clang".
		const float Into = FVector::DotProduct(HandVel, N);
		if (Into < 0.f)
		{
			HandVel -= (1.f + HardSurfaceRestitution) * Into * N;
		}
		HandPos += N * 0.5f;
	}

	// Shove physics props and ragdolls (BoneName means you can knock a specific limb).
	if (UPrimitiveComponent* Comp = Hit.GetComponent(); Comp && Comp->IsSimulatingPhysics(Hit.BoneName))
	{
		Comp->AddImpulseAtLocation(PointVel * MassKg * ImpulseScale, Hit.ImpactPoint, Hit.BoneName);
	}

	OnMeleeImpact.Broadcast(Hit, ImpactSpeed, bFlesh);
}
