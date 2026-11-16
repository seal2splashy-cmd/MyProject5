// OperatorArmsComponent.h
// First-person "Containment Operator" arms, posed entirely from C++ (no animations, no AnimBP).
//
// Every frame it:
//   1. asks the gun / melee weapon where each hand should grip (world space)
//   2. moves the BODY (shoulders/torso) with inertia: it trails the camera when you turn or
//      move, bobs with your steps, breathes, turns into your swings and leans to keep reach
//   3. builds the hand orientation from the handle axis (index finger towards the head)
//   4. solves a two-bone IK (shoulder -> elbow -> wrist); the shoulder follows the hand a
//      little and the elbow drags behind fast hand movement
//   5. curls the fingers and thumb around the grip (tighter when swinging, trigger squeeze on fire)
// The hands always land exactly on the weapon; everything behind them is loose.
//
// The mesh is self-calibrating: forward / right / up and the unit scale are worked out from
// the bone positions, so it doesn't matter which axis convention the FBX came in with.
// Expected bones (Tools/OperatorArms/rig_and_export_fbx_fixed.py):
//   clavicle_X, upperarm_X, lowerarm_X, forearm_twist_X, wrist_X, hand_X,
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

	// ---------------- Looseness ----------------
	/** THE knob. 0 = rigid (arms welded to the camera), 1 = default, 2 = very floppy.
	 *  Scales everything in this section at once. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0, ClampMax = 3))
	float Looseness = 1.f;

	/** How fast the body catches up with the camera (Hz). Lower = heavier, lazier arms. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0.5, ClampMax = 8))
	float BodySpringHz = 2.6f;

	/** 1 = settles with no overshoot, lower = wobbles past and back. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0.15, ClampMax = 1.5))
	float BodySpringDamping = 0.5f;

	/** Seconds the shoulders trail behind when you turn the camera. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float TurnLagSeconds = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float MaxTurnLagDegrees = 12.f;

	/** Seconds the shoulders trail behind when you move. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float MoveLagSeconds = 0.004f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float MaxMoveLagCm = 6.f;

	/** How much the torso turns towards where the hands are (0..1). Makes wind-ups use the whole body. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0, ClampMax = 1))
	float TorsoFollow = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float MaxTorsoDegrees = 28.f;

	/** How much each shoulder moves towards its hand (0..1). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0, ClampMax = 1))
	float ShoulderFollow = 0.18f;

	/** Limit on that shoulder movement, as a fraction of the upper-arm length. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0, ClampMax = 0.5))
	float MaxShoulderShift = 0.22f;

	/** How much the elbow swings out behind a fast-moving hand. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float ElbowDrag = 0.003f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float BobHeightCm = 1.4f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float BobSwayCm = 1.2f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float BobRollDegrees = 2.f;

	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float BreathCm = 0.5f;

	/** How much the fingers relax when the hand is still (they tighten as it speeds up). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0, ClampMax = 0.35))
	float GripSqueeze = 0.1f;

	/** Backwards jolt through the shoulders per shot (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	float RecoilKick = 55.f;

	/** The body leans towards a hand that's further than ComfortReach, so elbows stay bent
	 *  and hands stay on the weapon. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness")
	bool bLeanToReach = true;

	/** Fraction of full arm length the arms are happy to extend to (1 = dead straight).
	 *  Lower = more bent, relaxed elbows. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Looseness", meta = (ClampMin = 0.5, ClampMax = 1))
	float ComfortReach = 0.82f;

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

	/** Index finger curl at the moment of a trigger pull. */
	UPROPERTY(EditAnywhere, Category = "Operator Arms|Grip", meta = (ClampMin = 0, ClampMax = 1))
	float TriggerPullCurl = 0.8f;

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
		FName Clav, Upper, Lower, Twist, Wrist, Hand;
		FName Digit[NumDigits][3];

		// Reference pose, component space
		FVector C0, S, E0, M0, W0, H0, Mid1;
		FQuat RClav, RUpper, RLower, RTwist, RWrist, RHand;
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
	int32 Source[2] = { -1, -1 };   // -1 unset, 0 idle, 1 melee, 2 gun

	FQuat MeshBasis = FQuat::Identity;   // mesh (fwd, right, up) -> component
	FVector ShoulderMid = FVector::ZeroVector;
	FVector MeshDown = -FVector::UpVector;
	float ShoulderHalfWidth = 20.f;      // mesh units
	float UnitFix = 1.f;
	bool bReady = false;

	// ---- body dynamics (camera space; cm and degrees) ----
	FVector LagPos = FVector::ZeroVector, LagPosVel = FVector::ZeroVector;
	FVector LagRot = FVector::ZeroVector, LagRotVel = FVector::ZeroVector;   // (pitch, yaw, roll)
	FVector Lean = FVector::ZeroVector;
	FVector2D Torso = FVector2D::ZeroVector;                                 // (pitch, yaw)
	FVector PrevCamPos = FVector::ZeroVector;
	FQuat PrevCamQuat = FQuat::Identity;
	bool bHasPrevCam = false;
	float BobPhase = 0.f, MoveAlpha = 0.f, BreathTime = 0.f;
	FVector HandVel[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector PrevGrip[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bHasPrevGrip[2] = { false, false };
	int32 LastAmmo = -1;

	TWeakObjectPtr<AGun> Gun;
	TWeakObjectPtr<UMeleeCombatComponent> Melee;

	bool CalibrateRig();
	void ResetDynamics();
	void UpdateBody(float Dt);
	void GatherTargets(FHandTarget OutTargets[2], int32 OutSources[2]) const;
	void SolveArm(const FArmRig& A, bool bRight, const FHandTarget& T, const FVector& ElbowExtraCam, float Squeeze);
	void SetBoneRotationCS(FName Bone, const FQuat& Q);
};
