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
}

// ------------------------------------------------------------------ setup

bool UOperatorArmsComponent::InitOperatorArms()
{
	bReady = false;

	USkeletalMesh* MeshAsset = OperatorMesh.LoadSynchronous();
	if (!MeshAsset)
	{
		UE_LOG(LogTemp, Log, TEXT("OperatorArms: %s isn't imported yet - run Tools/ue_setup_project.py. Using the old arms."),
			*OperatorMesh.ToString());
		SetVisibility(false);
		return false;
	}

	SetSkinnedAssetAndUpdate(MeshAsset, true);

	if (!CalibrateRig())
	{
		SetVisibility(false);
		return false;
	}

	bReady = true;
	Source[0] = Source[1] = -1;
	SetVisibility(true);
	UpdatePlacement();
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

		A.Upper = FName(*Name(TEXT("upperarm")));
		A.Lower = FName(*Name(TEXT("lowerarm")));
		A.Twist = FName(*Name(TEXT("forearm_twist")));
		A.Wrist = FName(*Name(TEXT("wrist")));
		A.Hand  = FName(*Name(TEXT("hand")));

		FTransform TU, TL, TT, TW, TH;
		Find(A.Upper.ToString(), TU);
		Find(A.Lower.ToString(), TL);
		Find(A.Twist.ToString(), TT);
		Find(A.Wrist.ToString(), TW);
		Find(A.Hand.ToString(), TH);

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

void UOperatorArmsComponent::UpdatePlacement()
{
	// Component sits on the camera, rotated so the mesh faces the camera's forward, and
	// offset so the shoulders land at ShoulderOffset. Recomputed every tick so it's live-tunable.
	const float Scale = ArmsScale * UnitFix;
	const FQuat RelRot = MeshBasis.Inverse();
	const FVector RelLoc = ShoulderOffset - RelRot.RotateVector(ShoulderMid * Scale);
	SetRelativeTransform(FTransform(RelRot, RelLoc, FVector(Scale)));
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
			OutTargets[0] = { ToCam(R), AxisCam, 1.f, TriggerFingerCurl };
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

	UpdatePlacement();

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
		Blend[i] = FMath::Min(1.f, Blend[i] + DeltaTime / SwitchBlendTime);
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

	// camera space -> component space, then solve
	const FTransform Rel = GetRelativeTransform();
	for (int32 i = 0; i < 2; ++i)
	{
		FHandTarget T = Out[i];
		T.Grip = Rel.InverseTransformPosition(Out[i].Grip);
		T.Axis = Rel.GetRotation().UnrotateVector(Out[i].Axis);
		SolveArm(Arms[i], i == 0, T);
	}

	RefreshBoneTransforms();
}

// ------------------------------------------------------------------ IK

void UOperatorArmsComponent::SolveArm(const FArmRig& A, bool bRight, const FHandTarget& T)
{
	const FVector MeshFwd = MeshBasis.GetAxisX();
	const FVector Axis = T.Axis.GetSafeNormal(UE_SMALL_NUMBER, -MeshDown);

	// 1. Hand orientation: index->pinky runs down the handle, knuckles roughly along the arm
	const FVector Hint = (T.Grip - A.S).GetSafeNormal(UE_SMALL_NUMBER, MeshFwd);
	FVector HandFwd = Hint - Axis * FVector::DotProduct(Hint, Axis);
	if (!HandFwd.Normalize())
	{
		HandFwd = MeshFwd;
	}
	const FQuat QHand = FRotationMatrix::MakeFromXY(HandFwd, -Axis).ToQuat() * A.HandBasisRest.Inverse();

	// 2. Put the wrist wherever makes the fist centre land on the grip
	const FVector GripRest = FMath::Lerp(A.H0, A.Mid1, GripAlong) + MeshDown * (GripPalmDepth * A.PalmLen);
	FVector Wrist = T.Grip - QHand.RotateVector(GripRest - A.W0);

	// 3. Two-bone IK, shoulder fixed
	const FVector ToWrist = Wrist - A.S;
	float Dist = ToWrist.Size();
	const FVector Dir = (Dist > KINDA_SMALL_NUMBER) ? ToWrist / Dist : MeshFwd;
	Dist = FMath::Clamp(Dist, FMath::Abs(A.L1 - A.L2) + 0.01f * A.L1, (A.L1 + A.L2) * 0.999f);
	Wrist = A.S + Dir * Dist;

	const float Along = (A.L1 * A.L1 - A.L2 * A.L2 + Dist * Dist) / (2.f * Dist);
	const float Height = FMath::Sqrt(FMath::Max(A.L1 * A.L1 - Along * Along, 0.f));

	FVector PoleCam = ElbowPole;
	if (!bRight)
	{
		PoleCam.Y = -PoleCam.Y;
	}
	const FVector Pole = GetRelativeTransform().GetRotation().UnrotateVector(PoleCam.GetSafeNormal());
	FVector PoleDir = Pole - Dir * FVector::DotProduct(Pole, Dir);
	if (!PoleDir.Normalize())
	{
		PoleDir = MeshDown;
	}
	const FVector Elbow = A.S + Dir * Along + PoleDir * Height;

	const FQuat QUpper = FQuat::FindBetweenVectors(A.E0 - A.S, Elbow - A.S);
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

	// 4. Curl fingers (planar chain about one axis per digit), thumb opposes towards the palm
	for (int32 d = 0; d < NumDigits; ++d)
	{
		const float Curl = (d == Index) ? T.IndexCurl : T.Curl;
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
