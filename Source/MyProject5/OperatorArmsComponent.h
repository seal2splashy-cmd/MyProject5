// OperatorArmsComponent.h
// First-person "Containment Operator" arms, posed entirely from C++ (no animations, no AnimBP).
//
// Every frame it:
//   1. asks the gun / melee weapon where each hand should grip (world space)
//   2. builds the hand orientation from the handle axis (index finger towards the head)
//   3. solves a two-bone IK (shoulder -> elbow -> wrist) with an elbow pole
//   4. curls the fingers and thumb around the grip
// Because the hands chase the melee weapon's physics grip, they swing *with* the weapon.
//
// The mesh is self-calibrating: forward / right / up and the unit scale are worked out from
// the bone positions, so it doesn't matter which axis convention the FBX came in with.
// Expected bones (Tools/OperatorArms/rig_and_export_fbx_fixed.py):
//   upperarm_X, lowerarm_X, forearm_twist_X, wrist_X, hand_X,
//   index/middle/ring/pinky/thumb_01..03_X       (X = r / l)

#pragma once

#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "OperatorArmsComponent.generated.h"

class AGun;
class UMeleeCombatComponent;
class USkeletalMesh;

UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class MYPROJECT5_API UOperatorArmsComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UOperatorArmsComponent();

	/** Loads the mesh and calibrates the rig. Returns false (and stays hidden) if anything is missing. */
	bool InitOperatorArms();

	void SetSources(AGun* InGun, UMeleeCombatComponent* InMelee);

	bool IsOperatorReady() const { return bReady; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------- Setup ----------------
	/** Imported by Tools/ue_setup_project.py. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms")
	TSoftObjectPtr<USkeletalMesh> OperatorMesh;

	/** 1 = the mesh's authored size. The authored arms are long, so they're scaled down a bit. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms", meta = (ClampMin = 0.3, ClampMax = 2))
	float ArmsScale = 0.75f;

	/** Where the midpoint between the shoulders sits, camera space (X fwd, Y right, Z up), cm. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms")
	FVector ShoulderOffset = FVector(-15.f, 0.f, -25.f);

	/** Direction the RIGHT elbow points, camera space (mirrored for the left). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms")
	FVector ElbowPole = FVector(-0.3f, 0.6f, -1.f);

	// ---------------- Grip shape ----------------
	/** How far from the wrist to the knuckles the fist centre is (0..1 of palm length). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip")
	float GripAlong = 0.85f;

	/** How far below the palm the fist centre is (fraction of palm length). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip")
	float GripPalmDepth = 0.35f;

	/** Bend per finger joint for a full fist (knuckle, middle, tip), degrees. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip")
	FVector JointCurlDegrees = FVector(55.f, 70.f, 45.f);

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip", meta = (ClampMin = 0, ClampMax = 1))
	float ThumbCurlScale = 0.45f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip")
	float ThumbOppositionDegrees = 35.f;

	/** Right index finger curl while holding the pistol (resting on the trigger). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip", meta = (ClampMin = 0, ClampMax = 1))
	float TriggerFingerCurl = 0.45f;

	/** Curl for a hand that isn't holding anything. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip", meta = (ClampMin = 0, ClampMax = 1))
	float RelaxedCurl = 0.25f;

	/** How much of the hand's roll the forearm twist bone takes (stops candy-wrapper wrists). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip", meta = (ClampMin = 0, ClampMax = 1))
	float TwistShare = 0.6f;

	/** Free hand rest position, camera space (left hand; mirrored for the right). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip")
	FVector IdleHandOffset = FVector(30.f, -20.f, -42.f);

	/** Seconds to move a hand to a new grip when you switch weapons. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip", meta = (ClampMin = 0.01))
	float SwitchBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Debug")
	bool bDrawDebug = false;

private:
	enum EDigit { Index = 0, Middle, Ring, Pinky, Thumb, NumDigits };

	struct FArmRig
	{
		FName Upper, Lower, Twist, Wrist, Hand;
		FName Digit[NumDigits][3];

		// Reference pose, component space
		FVector S, E0, M0, W0, H0, Mid1;
		FQuat RUpper, RLower, RTwist, RWrist, RHand;
		FQuat RDigit[NumDigits][3];
		FVector DigitAxis[NumDigits];      // curl axis per digit
		FVector ThumbOppAxis;
		FQuat HandBasisRest;
		FVector TwistAxis;
		float L1 = 0.f, L2 = 0.f;
		float PalmLen = 0.f;
	};

	struct FHandTarget   // camera space
	{
		FVector Grip = FVector::ZeroVector;
		FVector Axis = FVector::UpVector;
		float Curl = 0.f;
		float IndexCurl = 0.f;
	};

	FArmRig Arms[2];                // 0 = right, 1 = left
	FHandTarget Out[2], From[2];
	float Blend[2] = { 1.f, 1.f };
	int32 Source[2] = { -1, -1 };

	FQuat MeshBasis = FQuat::Identity;   // mesh (fwd, right, up) -> component
	FVector ShoulderMid = FVector::ZeroVector;
	FVector MeshDown = -FVector::UpVector;
	float UnitFix = 1.f;
	bool bReady = false;

	TWeakObjectPtr<AGun> Gun;
	TWeakObjectPtr<UMeleeCombatComponent> Melee;

	bool CalibrateRig();
	void UpdatePlacement();
	void GatherTargets(FHandTarget OutTargets[2], int32 OutSources[2]) const;
	void SolveArm(const FArmRig& A, bool bRight, const FHandTarget& T);
	void SetBoneRotationCS(FName Bone, const FQuat& Q);
};
