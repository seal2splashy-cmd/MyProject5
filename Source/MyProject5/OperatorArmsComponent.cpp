// OperatorArmsComponent.cpp

#include "OperatorArmsComponent.h"

#include "AGun.h"
#include "MeleeCombatComponent.h"
#include "MeleeWeapon.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "ReferenceSkeleton.h"

UOperatorArmsComponent::UOperatorArmsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// after the pawn (camera), the gun's sway and the melee weapon's physics have all updated
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bCastDynamicShadow = false;
	SetOnlyOwnerSee(true);
	BoundsScale = 3.f;   // the posed arms leave the reference-pose bounds; don't let them get culled

	OperatorMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Generated/Operator/SK_AH_ContainmentOperator_FP.SK_AH_ContainmentOperator_FP")));
	MeshyArmsMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Generated/Operator/SK_AH_OperatorArms_Meshy.SK_AH_OperatorArms_Meshy")));
}

// ------------------------------------------------------------------ setup

bool UOperatorArmsComponent::InitOperatorArms()
{
	bReady = false;

	// The Meshy arms if they're imported (checked first so a missing package doesn't log a load
	// warning), else the original operator arms. Same skeleton, so everything below is shared.
	USkeletalMesh* MeshAsset = nullptr;
	if (bPreferMeshyArms && !MeshyArmsMesh.IsNull() &&
		FPackageName::DoesPackageExist(MeshyArmsMesh.ToSoftObjectPath().GetLongPackageName()))
	{
		MeshAsset = MeshyArmsMesh.LoadSynchronous();
	}
	if (!MeshAsset)
	{
		MeshAsset = OperatorMesh.LoadSynchronous();
	}
	if (!MeshAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("OperatorArms: %s isn't imported yet - run Tools/ue_setup_project.py. Using the old arms."),
			*OperatorMesh.ToString());
		SetVisibility(false);
		return false;
	}

	SetSkinnedAssetAndUpdate(MeshAsset, true);
	UE_LOG(LogTemp, Log, TEXT("OperatorArms: using %s"), *MeshAsset->GetName());

	if (!CalibrateRig())
	{
		SetVisibility(false);
		return false;
	}

	bReady = true;
	Source[0] = Source[1] = -1;
	ResetDynamics();
	SetVisibility(true);
	UpdateBody(0.f);
	return true;
}

void UOperatorArmsComponent::SetSources(AGun* InGun, UMeleeCombatComponent* InMelee)
{
	Gun = InGun;
	Melee = InMelee;
}

bool UOperatorArmsComponent::CalibrateRig()
{
	const USkinnedAsset* Asset = GetSkinnedAsset();
	if (!Asset)
	{
		return false;
	}

	// Reference pose in component space
	const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
	const TArray<FTransform>& Local = Ref.GetRefBonePose();
	TArray<FTransform> CS;
	CS.SetNum(Local.Num());
	for (int32 i = 0; i < Local.Num(); ++i)
	{
		const int32 Parent = Ref.GetParentIndex(i);
		CS[i] = (Parent == INDEX_NONE) ? Local[i] : Local[i] * CS[Parent];
	}

	TArray<FString> Missing;
	auto Find = [&](const FString& Name, FTransform& OutT)
	{
		const int32 Index = Ref.FindBoneIndex(FName(*Name));
		if (Index == INDEX_NONE)
		{
			Missing.Add(Name);
			OutT = FTransform::Identity;
			return;
		}
		OutT = CS[Index];
	};

	static const TCHAR* DigitNames[NumDigits] = { TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky"), TEXT("thumb") };
	FVector DigitHead[2][NumDigits][3];

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const TCHAR* X = (Side == 0) ? TEXT("r") : TEXT("l");
		FArmRig& A = Arms[Side];
		auto Name = [X](const TCHAR* Base) { return FString::Printf(TEXT("%s_%s"), Base, X); };

		A.Clav  = FName(*Name(TEXT("clavicle")));
		A.Upper = FName(*Name(TEXT("upperarm")));
		A.Lower = FName(*Name(TEXT("lowerarm")));
		A.Twist = FName(*Name(TEXT("forearm_twist")));
		A.Wrist = FName(*Name(TEXT("wrist")));
		A.Hand  = FName(*Name(TEXT("hand")));

		FTransform TC, TU, TL, TT, TW, TH;
		Find(A.Clav.ToString(), TC);
		Find(A.Upper.ToString(), TU);
		Find(A.Lower.ToString(), TL);
		Find(A.Twist.ToString(), TT);
		Find(A.Wrist.ToString(), TW);
		Find(A.Hand.ToString(), TH);

		A.C0 = TC.GetLocation();  A.RClav  = TC.GetRotation();
		A.S  = TU.GetLocation();  A.RUpper = TU.GetRotation();
		A.E0 = TL.GetLocation();  A.RLower = TL.GetRotation();
		A.M0 = TT.GetLocation();  A.RTwist = TT.GetRotation();
		A.W0 = TW.GetLocation();  A.RWrist = TW.GetRotation();
		A.H0 = TH.GetLocation();  A.RHand  = TH.GetRotation();

		for (int32 d = 0; d < NumDigits; ++d)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				const FString BoneName = FString::Printf(TEXT("%s_%02d_%s"), DigitNames[d], j + 1, X);
				A.Digit[d][j] = FName(*BoneName);
				FTransform TD;
				Find(BoneName, TD);
				DigitHead[Side][d][j] = TD.GetLocation();
				A.RDigit[d][j] = TD.GetRotation();
			}
		}
	}

	if (Missing.Num() > 0)
	{
		UE_LOG(LogTemp, Error, TEXT("OperatorArms: mesh is missing bones: %s"), *FString::Join(Missing, TEXT(", ")));
		return false;
	}

	// Work out the mesh's own forward / right / up from the bones, so any import axis works
	FVector Right = Arms[0].S - Arms[1].S;
	const float ShoulderWidth = Right.Size();
	if (ShoulderWidth < KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Error, TEXT("OperatorArms: shoulders are on top of each other - bad rig?"));
		return false;
	}
	Right /= ShoulderWidth;
	FVector Fwd = (Arms[0].W0 - Arms[0].S) + (Arms[1].W0 - Arms[1].S);
	Fwd = (Fwd - Right * FVector::DotProduct(Fwd, Right)).GetSafeNormal();
	const FVector Up = FVector::CrossProduct(Fwd, Right);   // UE: X(fwd) x Y(right) = Z(up)
	MeshDown = -Up;
	MeshBasis = FRotationMatrix::MakeFromXY(Fwd, Right).ToQuat();
	ShoulderMid = (Arms[0].S + Arms[1].S) * 0.5f;
	ShoulderHalfWidth = ShoulderWidth * 0.5f;

	// The authored rig has 41 cm between shoulders. If the FBX came in metres (or 100x), fix it.
	UnitFix = 41.f / ShoulderWidth;
	if (FMath::Abs(UnitFix - 1.f) > 0.1f)
	{
		UE_LOG(LogTemp, Warning, TEXT("OperatorArms: shoulder width %.3f units, applying x%.3f unit fix"), ShoulderWidth, UnitFix);
	}
	else
	{
		UnitFix = 1.f;
	}

	for (int32 Side = 0; Side < 2; ++Side)
	{
		FArmRig& A = Arms[Side];
		A.L1 = (A.E0 - A.S).Size();
		A.L2 = (A.W0 - A.E0).Size();
		A.Mid1 = DigitHead[Side][Middle][0];
		A.PalmLen = (A.Mid1 - A.H0).Size();
		A.TwistAxis = (A.W0 - A.M0).GetSafeNormal();
		// rest hand frame: X = wrist->knuckles, Y = index->pinky
		A.HandBasisRest = FRotationMatrix::MakeFromXY(A.Mid1 - A.H0,
			DigitHead[Side][Pinky][0] - DigitHead[Side][Index][0]).ToQuat();

		for (int32 d = 0; d < NumDigits; ++d)
		{
			const FVector Dir = (DigitHead[Side][d][1] - DigitHead[Side][d][0]).GetSafeNormal();
			// rotating +angle about (dir x down) swings the finger towards the palm
			A.DigitAxis[d] = FVector::CrossProduct(Dir, MeshDown).GetSafeNormal();
		}

		const FVector ThumbDir = (DigitHead[Side][Thumb][1] - DigitHead[Side][Thumb][0]).GetSafeNormal();
		FVector ToCentre = FMath::Lerp(A.H0, A.Mid1, 0.6f) - DigitHead[Side][Thumb][0];
		ToCentre -= ThumbDir * FVector::DotProduct(ToCentre, ThumbDir);
		A.ThumbOppAxis = FVector::CrossProduct(ThumbDir, ToCentre).GetSafeNormal();

		if (A.L1 < KINDA_SMALL_NUMBER || A.L2 < KINDA_SMALL_NUMBER || A.PalmLen < KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogTemp, Error, TEXT("OperatorArms: degenerate arm chain on side %d"), Side);
			return false;
		}
	}

	return true;
}

void UOperatorArmsComponent::ResetDynamics()
{
	LagPos = LagPosVel = LagRot = LagRotVel = Lean = FVector::ZeroVector;
	Torso = FVector2D::ZeroVector;
	HandVel[0] = HandVel[1] = FVector::ZeroVector;
	bHasPrevGrip[0] = bHasPrevGrip[1] = false;
	bHasPrevCam = false;
	MoveAlpha = 0.f;
	LastAmmo = -1;
}

// ------------------------------------------------------------------ body

void UOperatorArmsComponent::UpdateBody(float Dt)
{
	// The component hangs off the camera. Instead of welding the shoulders to it, the body is a
	// spring: it trails the camera when you turn or move, bobs, breathes, turns towards the hands
	// and leans to keep them in reach. The hands are IK'd afterwards, so they stay on the weapon.
	const float Scale = ArmsScale * UnitFix;
	const float L = Looseness;

	const USceneComponent* Cam = GetAttachParent();
	if (Cam && Dt > KINDA_SMALL_NUMBER)
	{
		const FVector CamPos = Cam->GetComponentLocation();
		const FQuat CamQuat = Cam->GetComponentQuat();

		FVector TargetPos = FVector::ZeroVector;   // cm
		FVector TargetRot = FVector::ZeroVector;   // pitch, yaw, roll (degrees)
		float PlanarSpeed = 0.f;

		if (bHasPrevCam && FVector::DistSquared(CamPos, PrevCamPos) < FMath::Square(300.f))
		{
			// how the camera moved this frame, in its own frame
			const FVector VelLocal = CamQuat.UnrotateVector((CamPos - PrevCamPos) / Dt);
			const FRotator Turn = (PrevCamQuat.Inverse() * CamQuat).Rotator();
			const float RateToLag = TurnLagSeconds / Dt;
			const float MaxDeg = MaxTurnLagDegrees;

			// trail behind: opposite to the motion
			TargetPos = (VelLocal * -MoveLagSeconds).GetClampedToMaxSize(MaxMoveLagCm);
			TargetRot.X = FMath::Clamp(-static_cast<float>(Turn.Pitch) * RateToLag, -MaxDeg, MaxDeg);
			TargetRot.Y = FMath::Clamp(-static_cast<float>(Turn.Yaw) * RateToLag, -MaxDeg, MaxDeg);
			TargetRot.Z = FMath::Clamp(-static_cast<float>(Turn.Roll) * RateToLag * 0.5f, -MaxDeg, MaxDeg);
			PlanarSpeed = static_cast<float>(FVector2D(VelLocal.X, VelLocal.Y).Size());
		}
		else
		{
			// first frame, or a teleport/respawn: don't whip the arms across the screen
			LagPos = LagPosVel = LagRot = LagRotVel = FVector::ZeroVector;
		}
		PrevCamPos = CamPos;
		PrevCamQuat = CamQuat;
		bHasPrevCam = true;

		// Spring-damper towards the trailing target (sub-stepped so it's stable at low fps)
		const float W = 2.f * UE_PI * BodySpringHz;
		const float K = W * W;
		const float C = 2.f * BodySpringDamping * W;
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Dt / 0.012f), 1, 8);
		const float H = Dt / static_cast<float>(Steps);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			LagPosVel += ((TargetPos * L - LagPos) * K - LagPosVel * C) * H;
			LagPos += LagPosVel * H;
			LagRotVel += ((TargetRot * L - LagRot) * K - LagRotVel * C) * H;
			LagRot += LagRotVel * H;
		}
		const float Slack = 2.f * FMath::Max(L, 1.f);
		LagPos = LagPos.GetClampedToMaxSize(MaxMoveLagCm * Slack);
		LagRot = LagRot.GetClampedToMaxSize(MaxTurnLagDegrees * Slack);

		// Walk bob + breathing
		MoveAlpha = FMath::FInterpTo(MoveAlpha, FMath::Clamp(PlanarSpeed / 600.f, 0.f, 1.f), Dt, 8.f);
		if (MoveAlpha > 0.02f)
		{
			BobPhase = FMath::Fmod(BobPhase + Dt * 10.f, 4.f * UE_PI);
		}
		BreathTime += Dt;

		// Torso turns towards where the hands are, so a wind-up uses the whole body
		FVector Mid = FVector::ZeroVector;
		int32 Count = 0;
		for (int32 i = 0; i < 2; ++i)
		{
			if (Source[i] > 0)
			{
				Mid += Out[i].Grip;
				++Count;
			}
		}
		FVector2D TorsoTarget = FVector2D::ZeroVector;
		if (Count > 0)
		{
			const FVector D = Mid / static_cast<float>(Count) - ShoulderOffset;
			const float Follow = FMath::Min(TorsoFollow * L, 0.9f);
			const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Y, FMath::Max(D.X, 1.0))));
			const float Pitch = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(D.Z, FMath::Max(D.Size2D(), 1.0))));
			TorsoTarget.X = FMath::Clamp(Pitch * Follow * 0.5f, -MaxTorsoDegrees * 0.5f, MaxTorsoDegrees * 0.5f);
			TorsoTarget.Y = FMath::Clamp(Yaw * Follow, -MaxTorsoDegrees, MaxTorsoDegrees);
		}
		Torso = FMath::Vector2DInterpTo(Torso, TorsoTarget, Dt, 14.f);

		// Lean in when a hand gets further than a comfortable reach: keeps the elbows bent
		// (straight, locked-out arms are what reads as "stiff") and the hands on the weapon
		FVector LeanTarget = FVector::ZeroVector;
		if (bLeanToReach)
		{
			for (int32 i = 0; i < 2; ++i)
			{
				if (Source[i] <= 0)
				{
					continue;
				}
				const FArmRig& A = Arms[i];
				const FVector GripRest = FMath::Lerp(A.H0, A.Mid1, GripAlong) + MeshDown * (GripPalmDepth * A.PalmLen);
				const float HandLen = 0.8f * static_cast<float>((GripRest - A.W0).Size()) * Scale;
				const float Reach = ComfortReach * (A.L1 + A.L2) * Scale;
				const FVector ShoulderCam = ShoulderOffset + FVector(0.f, (i == 0 ? 1.f : -1.f) * ShoulderHalfWidth * Scale, 0.f);
				const FVector ToGrip = Out[i].Grip - ShoulderCam;
				const float Over = static_cast<float>(ToGrip.Size()) - HandLen - Reach;
				if (Over > 0.f)
				{
					LeanTarget += ToGrip.GetSafeNormal() * Over;
				}
			}
		}
		Lean = FMath::VInterpTo(Lean, LeanTarget, Dt, 25.f);
	}

	const float BobAmount = MoveAlpha * L;
	FVector Bob(0.f,
		FMath::Sin(BobPhase * 0.5f) * BobSwayCm * BobAmount,
		FMath::Sin(BobPhase) * BobHeightCm * BobAmount);
	Bob.Z += FMath::Sin(BreathTime * 1.5f) * BreathCm * L;
	const float BobRoll = FMath::Sin(BobPhase * 0.5f) * BobRollDegrees * BobAmount;

	// Rotate about the point between the shoulders, which sits at ShoulderOffset (+ all the offsets)
	const FQuat BodyQ = FRotator(LagRot.X + Torso.X, LagRot.Y + Torso.Y, LagRot.Z + BobRoll).Quaternion();
	const FQuat RelRot = BodyQ * MeshBasis.Inverse();
	const FVector Pivot = ShoulderOffset + LagPos + Bob + Lean;
	SetRelativeTransform(FTransform(RelRot, Pivot - RelRot.RotateVector(ShoulderMid * Scale), FVector(Scale)));
}

// ------------------------------------------------------------------ targets

void UOperatorArmsComponent::GatherTargets(FHandTarget OutTargets[2], int32 OutSources[2]) const
{
	const USceneComponent* Cam = GetAttachParent();
	const FTransform CamT = Cam ? Cam->GetComponentTransform() : GetComponentTransform();
	auto ToCam = [&CamT](const FVector& World) { return CamT.InverseTransformPosition(World); };
	auto ToCamDir = [&CamT](const FVector& World) { return CamT.InverseTransformVectorNoScale(World).GetSafeNormal(); };

	// Default: free hands hang low, slightly curled
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FVector Idle = IdleHandOffset;
		if (Side == 0) Idle.Y = -Idle.Y;
		OutTargets[Side].Grip = Idle;
		OutTargets[Side].Axis = FVector(0.3f, Side == 0 ? -0.2f : 0.2f, 1.f).GetSafeNormal();
		OutTargets[Side].Curl = RelaxedCurl;
		OutTargets[Side].IndexCurl = RelaxedCurl;
		OutSources[Side] = 0;
	}

	const UMeleeCombatComponent* MeleeComp = Melee.Get();
	const AMeleeWeapon* Weapon = (MeleeComp && MeleeComp->IsMeleeActive()) ? MeleeComp->GetWeapon() : nullptr;
	if (Weapon)
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FVector P, Axis;
			if (Weapon->GetHandGrip(Side == 0, P, Axis))
			{
				OutTargets[Side].Grip = ToCam(P);
				OutTargets[Side].Axis = ToCamDir(Axis);
				OutTargets[Side].Curl = 1.f;
				OutTargets[Side].IndexCurl = 1.f;
				OutSources[Side] = 1;
			}
		}
		return;
	}

	if (const AGun* G = Gun.Get())
	{
		FVector R, L, Axis;
		if (G->GetHandTargets(R, L, Axis))
		{
			const FVector AxisCam = ToCamDir(Axis);

			// index finger rests on the trigger and squeezes for a moment on each pull
			float IndexCurl = TriggerFingerCurl;
			if (const UWorld* World = GetWorld())
			{
				const float Since = static_cast<float>(World->GetTimeSeconds()) - G->GetLastFireTime();
				if (Since >= 0.f && Since < 0.15f)
				{
					IndexCurl = FMath::Lerp(TriggerPullCurl, TriggerFingerCurl, Since / 0.15f);
				}
			}

			OutTargets[0] = { ToCam(R), AxisCam, 1.f, IndexCurl };
			OutTargets[1] = { ToCam(L), AxisCam, 1.f, 1.f };
			OutSources[0] = OutSources[1] = 2;
		}
	}
}

// ------------------------------------------------------------------ tick

void UOperatorArmsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bReady || !IsVisible())
	{
		return;
	}

	const float Dt = FMath::Min(DeltaTime, 0.1f);

	FHandTarget Targets[2];
	int32 Sources[2];
	GatherTargets(Targets, Sources);

	// Blend in camera space when a hand changes what it's holding; otherwise track exactly
	for (int32 i = 0; i < 2; ++i)
	{
		if (Source[i] == -1)
		{
			Out[i] = Targets[i];
			Blend[i] = 1.f;
			Source[i] = Sources[i];
			continue;
		}
		if (Sources[i] != Source[i])
		{
			From[i] = Out[i];
			Blend[i] = 0.f;
			Source[i] = Sources[i];
		}
		Blend[i] = FMath::Min(1.f, Blend[i] + Dt / SwitchBlendTime);
		if (Blend[i] >= 1.f)
		{
			Out[i] = Targets[i];
		}
		else
		{
			const float A = FMath::SmoothStep(0.f, 1.f, Blend[i]);
			Out[i].Grip = FMath::Lerp(From[i].Grip, Targets[i].Grip, A);
			Out[i].Axis = FMath::Lerp(From[i].Axis, Targets[i].Axis, A).GetSafeNormal(UE_SMALL_NUMBER, Targets[i].Axis);
			Out[i].Curl = FMath::Lerp(From[i].Curl, Targets[i].Curl, A);
			Out[i].IndexCurl = FMath::Lerp(From[i].IndexCurl, Targets[i].IndexCurl, A);
		}
	}

	// How fast each hand is moving (camera space) - drives elbow drag and grip tightness
	for (int32 i = 0; i < 2; ++i)
	{
		if (bHasPrevGrip[i] && Dt > KINDA_SMALL_NUMBER)
		{
			HandVel[i] = FMath::VInterpTo(HandVel[i], (Out[i].Grip - PrevGrip[i]) / Dt, Dt, 10.f);
		}
		PrevGrip[i] = Out[i].Grip;
		bHasPrevGrip[i] = true;
	}

	// A shot jolts the shoulders back and up
	if (const AGun* G = Gun.Get())
	{
		const int32 Ammo = G->GetAmmoInMag();
		if (LastAmmo >= 0 && Ammo < LastAmmo && Source[0] == 2)
		{
			LagPosVel.X -= RecoilKick * Looseness;
			LagRotVel.X += RecoilKick * 2.5f * Looseness;
		}
		LastAmmo = Ammo;
	}

	UpdateBody(Dt);

	// camera space -> component space, then solve
	const FTransform Rel = GetRelativeTransform();
	for (int32 i = 0; i < 2; ++i)
	{
		FHandTarget T = Out[i];
		T.Grip = Rel.InverseTransformPosition(Out[i].Grip);
		T.Axis = Rel.GetRotation().UnrotateVector(Out[i].Axis);

		const FVector ElbowExtra = (HandVel[i] * (-ElbowDrag * Looseness)).GetClampedToMaxSize(0.6f);
		const float Squeeze = FMath::Clamp(static_cast<float>(HandVel[i].Size()) / 300.f, 0.f, 1.f);
		SolveArm(Arms[i], i == 0, T, ElbowExtra, Squeeze);
	}

	RefreshBoneTransforms();
}

// ------------------------------------------------------------------ IK

void UOperatorArmsComponent::SolveArm(const FArmRig& A, bool bRight, const FHandTarget& T, const FVector& ElbowExtraCam, float Squeeze)
{
	const FVector MeshFwd = MeshBasis.GetAxisX();
	const FVector Axis = T.Axis.GetSafeNormal(UE_SMALL_NUMBER, -MeshDown);

	// 0. Shoulder: the clavicle swings so the shoulder follows the hand a little
	const FVector ClavRest = A.S - A.C0;
	const FVector Neutral = A.S + MeshFwd * (0.7f * (A.L1 + A.L2));
	const FVector Shift = ((T.Grip - Neutral) * FMath::Min(ShoulderFollow * Looseness, 0.6f))
		.GetClampedToMaxSize(MaxShoulderShift * A.L1);
	FQuat QClav = FQuat::Identity;
	FVector S = A.S;
	if (!Shift.IsNearlyZero() && ClavRest.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		QClav = FQuat::FindBetweenVectors(ClavRest, A.S + Shift - A.C0);
		S = A.C0 + QClav.RotateVector(ClavRest);   // clavicle keeps its length
	}
	SetBoneRotationCS(A.Clav, QClav * A.RClav);

	// 1. Hand orientation: index->pinky runs down the handle, knuckles roughly along the arm
	const FVector Hint = (T.Grip - S).GetSafeNormal(UE_SMALL_NUMBER, MeshFwd);
	FVector HandFwd = Hint - Axis * FVector::DotProduct(Hint, Axis);
	if (!HandFwd.Normalize())
	{
		HandFwd = MeshFwd;
	}
	const FQuat QHand = FRotationMatrix::MakeFromXY(HandFwd, -Axis).ToQuat() * A.HandBasisRest.Inverse();

	// 2. Put the wrist wherever makes the fist centre land on the grip
	const FVector GripRest = FMath::Lerp(A.H0, A.Mid1, GripAlong) + MeshDown * (GripPalmDepth * A.PalmLen);
	FVector Wrist = T.Grip - QHand.RotateVector(GripRest - A.W0);

	// 3. Two-bone IK from the (moved) shoulder
	const FVector ToWrist = Wrist - S;
	float Dist = ToWrist.Size();
	const FVector Dir = (Dist > KINDA_SMALL_NUMBER) ? ToWrist / Dist : MeshFwd;
	Dist = FMath::Clamp(Dist, FMath::Abs(A.L1 - A.L2) + 0.01f * A.L1, (A.L1 + A.L2) * 0.999f);
	Wrist = S + Dir * Dist;

	const float Along = (A.L1 * A.L1 - A.L2 * A.L2 + Dist * Dist) / (2.f * Dist);
	const float Height = FMath::Sqrt(FMath::Max(A.L1 * A.L1 - Along * Along, 0.f));

	// Elbow points down-and-out, and drags behind a fast-moving hand
	FVector PoleCam = ElbowPole.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f));
	if (!bRight)
	{
		PoleCam.Y = -PoleCam.Y;
	}
	PoleCam += ElbowExtraCam;
	const FVector Pole = GetRelativeTransform().GetRotation().UnrotateVector(PoleCam.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.f, 0.f, -1.f)));
	FVector PoleDir = Pole - Dir * FVector::DotProduct(Pole, Dir);
	if (!PoleDir.Normalize())
	{
		PoleDir = MeshDown;
	}
	const FVector Elbow = S + Dir * Along + PoleDir * Height;

	const FQuat QUpper = FQuat::FindBetweenVectors(A.E0 - A.S, Elbow - S);
	const FQuat QFore = FQuat::FindBetweenVectors(A.W0 - A.E0, Wrist - Elbow);

	// Share the hand's roll with the forearm twist bone
	FQuat Swing, Twist;
	(QFore.Inverse() * QHand).ToSwingTwist(A.TwistAxis, Swing, Twist);
	const FQuat QTwist = QFore * FQuat::Slerp(FQuat::Identity, Twist, TwistShare);

	SetBoneRotationCS(A.Upper, QUpper * A.RUpper);
	SetBoneRotationCS(A.Lower, QFore * A.RLower);
	SetBoneRotationCS(A.Twist, QTwist * A.RTwist);
	SetBoneRotationCS(A.Wrist, QHand * A.RWrist);
	SetBoneRotationCS(A.Hand, QHand * A.RHand);

	// 4. Curl fingers (planar chain about one axis per digit), thumb opposes towards the palm.
	//    A holding hand relaxes slightly when still and clamps down as it speeds up; every
	//    finger also drifts a touch so the hand never looks frozen.
	const float Relax = FMath::Min(GripSqueeze * Looseness, 0.35f) * (1.f - Squeeze);
	for (int32 d = 0; d < NumDigits; ++d)
	{
		float Curl = (d == Index) ? T.IndexCurl : T.Curl;
		if (T.Curl > 0.5f)
		{
			Curl *= 1.f - Relax;
		}
		Curl += FMath::Sin(BreathTime * 1.3f + static_cast<float>(d) * 1.7f + (bRight ? 0.f : 0.9f)) * 0.02f * Looseness;
		Curl = FMath::Clamp(Curl, 0.f, 1.1f);
		const float DigitScale = (d == Thumb) ? ThumbCurlScale : 1.f;
		float Cumulative = 0.f;
		for (int32 j = 0; j < 3; ++j)
		{
			Cumulative += FMath::DegreesToRadians(static_cast<float>(JointCurlDegrees[j])) * Curl * DigitScale;
			FQuat Q(A.DigitAxis[d], Cumulative);
			if (d == Thumb)
			{
				Q = FQuat(A.ThumbOppAxis, FMath::DegreesToRadians(ThumbOppositionDegrees) * Curl) * Q;
			}
			SetBoneRotationCS(A.Digit[d][j], QHand * Q * A.RDigit[d][j]);
		}
	}

	if (bDrawDebug)
	{
		const FTransform& C = GetComponentTransform();
		UWorld* World = GetWorld();
		DrawDebugSphere(World, C.TransformPosition(T.Grip), 1.5f, 8, FColor::Green, false, -1.f, SDPG_Foreground);
		DrawDebugSphere(World, C.TransformPosition(Elbow), 1.5f, 8, FColor::Orange, false, -1.f, SDPG_Foreground);
		DrawDebugLine(World, C.TransformPosition(T.Grip), C.TransformPosition(T.Grip + Axis * 10.f), FColor::Cyan, false, -1.f, SDPG_Foreground, 0.3f);
	}
}

void UOperatorArmsComponent::SetBoneRotationCS(FName Bone, const FQuat& Q)
{
	if (GetBoneIndex(Bone) == INDEX_NONE)
	{
		return;
	}
	// Keep the bone's current (hierarchy-driven) position, replace only its rotation
	FTransform T = GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
	T.SetRotation(Q.GetNormalized());
	SetBoneTransformByName(Bone, T, EBoneSpaces::ComponentSpace);
}
