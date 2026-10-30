// MeleeWeapon.h
// Physically-swayed melee weapon for a bodycam-style FPS.
//
// The weapon is NOT attached to the camera. Its grip is a mass on a spring that
// chases a target point in camera space. Turning, walking and (while guarding)
// moving the mouse all move that target, and the spring does the rest — so the
// sway you get from looking around and the attack itself are the same system.
//
// Damage comes from how fast the blade is actually moving into the thing it hits,
// measured by sweeping several points along the blade every physics sub-step.
//
// Mesh convention: grip at the actor root, blade along local +X, cutting edge facing +Z.
// BladeBase/BladeTip are children of the grip, so their positions are in grip space.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "MeleeWeapon.generated.h"

class UCameraComponent;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USoundBase;
class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMeleeImpact, const FHitResult&, Hit, float, ImpactSpeed, bool, bHitFlesh);

UCLASS()
class MYPROJECT5_API AMeleeWeapon : public AActor
{
	GENERATED_BODY()

public:
	AMeleeWeapon();

	virtual void Tick(float DeltaSeconds) override;

	/** Call once from the pawn right after spawning the weapon. */
	void InitialiseFor(UCameraComponent* InCamera);

	/** Feed the raw look delta here every frame, BEFORE applying it to the camera. */
	void AddMouseInput(const FVector2D& LookDelta);

	/** Multiply your camera look input by this (guarding steals most of the mouse for the blade). */
	float GetLookScale() const { return bGuarding ? LookScaleWhileGuarding : 1.f; }

	void BeginGuard();
	void EndGuard();
	void Thrust();

	/** For hand IK in the arms AnimBP — the grip is the actor root. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	FTransform GetGripTransform() const { return GetActorTransform(); }

	/** World-space speed of the blade tip this frame (cm/s). Good for motion blur, whoosh audio, stamina drain. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	float GetTipSpeed() const { return TipSpeed; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsGuarding() const { return bGuarding; }

	/** Name shown on the HUD. */
	const FString& GetDisplayName() const { return DisplayName; }

	/** Fires on every contact. Hook camera shake, hit-stop, sparks, blood and audio here. */
	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeImpact OnMeleeImpact;

protected:
	virtual void BeginPlay() override;

	/**
	 * Adds a visual part built from an engine basic shape (no import needed, so the
	 * orientation is always right). Call from a subclass constructor. Location/scale are
	 * in grip space; a cylinder's long axis is its local Z (use pitch -90 to point it along +X).
	 * MaterialPath is optional (e.g. a generated /Game/Generated/Materials/... material);
	 * if it doesn't exist the part falls back to the engine's tintable BasicShapeMaterial.
	 */
	UStaticMeshComponent* AddPart(FName Name, bool bCube, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale, const FLinearColor& Colour, const TCHAR* MaterialPath = nullptr);

	/** Restyle the default Mesh component (index 0 of the parts). */
	void SetMainPart(bool bCube, const FVector& Loc, const FRotator& Rot, const FVector& Scale,
		const FLinearColor& Colour, const TCHAR* MaterialPath = nullptr);

	void SetBladePoints(const FVector& Base, const FVector& Tip);

	// ---------- Look ----------
	UPROPERTY(EditAnywhere, Category = "Melee|Look")
	FString DisplayName = TEXT("Pipe");

	UPROPERTY(VisibleAnywhere, Category = "Melee|Look")
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;   // [0] = Mesh

	UPROPERTY(EditAnywhere, Category = "Melee|Look")
	TArray<FLinearColor> PartColours;

	UPROPERTY(EditAnywhere, Category = "Melee|Look")
	TArray<TSoftObjectPtr<UMaterialInterface>> PartMaterials;

	// ---------- Audio ----------
	UPROPERTY(EditAnywhere, Category = "Melee|Audio")
	TArray<TSoftObjectPtr<USoundBase>> WhooshSounds;

	UPROPERTY(EditAnywhere, Category = "Melee|Audio")
	TArray<TSoftObjectPtr<USoundBase>> FleshHitSounds;

	UPROPERTY(EditAnywhere, Category = "Melee|Audio")
	TArray<TSoftObjectPtr<USoundBase>> HardHitSounds;

	/** Hand speed relative to the camera (cm/s) that counts as a swing worth a whoosh. */
	UPROPERTY(EditAnywhere, Category = "Melee|Audio")
	float WhooshSpeed = 450.f;

	/** Below this into-surface speed a contact is silent (resting against a wall, etc). */
	UPROPERTY(EditAnywhere, Category = "Melee|Audio")
	float MinImpactSoundSpeed = 150.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Audio")
	float SoundVolume = 1.f;

	// ---------- Components ----------
	UPROPERTY(VisibleAnywhere, Category = "Melee")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Melee")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Move these in the BP to sit at the start and end of the cutting surface. */
	UPROPERTY(VisibleAnywhere, Category = "Melee")
	TObjectPtr<USceneComponent> BladeBase;

	UPROPERTY(VisibleAnywhere, Category = "Melee")
	TObjectPtr<USceneComponent> BladeTip;

	// ---------- Feel ----------
	/** Where the hand rests, camera space (X fwd, Y right, Z up), cm. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	FVector RestOffset = FVector(45.f, 22.f, -28.f);

	/** Added to RestOffset while guarding — brings the weapon up and towards centre. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	FVector GuardPose = FVector(5.f, -12.f, 14.f);

	/** Rough shoulder position, camera space. The blade leans away from it like a pendulum. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	FVector ShoulderOffset = FVector(-10.f, 20.f, -25.f);

	/** THE knob. Knife ~0.5, sword ~1.5, sledgehammer ~5. Heavier = laggier, slower, hits harder. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel", meta = (ClampMin = 0.1))
	float MassKg = 1.5f;

	/** How hard the arm pulls the grip towards its target. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel", meta = (ClampMin = 1))
	float Stiffness = 900.f;

	/** 1 = no overshoot, <1 = follow-through wobble. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel", meta = (ClampMin = 0.05, ClampMax = 2))
	float DampingRatio = 0.55f;

	/** How much the blade tilts up from the shoulder->hand line. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	float BladeUpBias = 0.8f;

	/** At this hand speed (cm/s) the edge fully turns to face the swing direction. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel", meta = (ClampMin = 1))
	float EdgeAlignSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	float RotationSharpness = 18.f;

	/** Fraction of mouse that still turns the camera while guarding. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel", meta = (ClampMin = 0, ClampMax = 1))
	float LookScaleWhileGuarding = 0.2f;

	/** cm of hand travel per unit of look input while guarding (tuned for legacy MouseX/MouseY axes). */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	float SwingSensitivity = 4.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	bool bInvertSwingY = false;

	/** Max distance (cm) the mouse can push the hand away from the guard pose. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	float ControlRadius = 40.f;

	/** kg*cm/s added along the camera's forward on Thrust(). */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	float ThrustImpulse = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Feel", meta = (ClampMin = 1, ClampMax = 16))
	int32 SubSteps = 4;

	/** If the camera jumps further than this in one frame (teleport/respawn), snap instead of whipping. */
	UPROPERTY(EditAnywhere, Category = "Melee|Feel")
	float TeleportSnapDistance = 300.f;

	// ---------- Combat ----------
	/** Speed INTO the surface (cm/s) needed to do any damage. Stops idle sway from hurting things. */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat")
	float MinDamageSpeed = 250.f;

	/** Damage per (kg * cm/s) above MinDamageSpeed. */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat")
	float DamagePerMomentum = 0.03f;

	/** Multiplier on the impulse given to physics bodies / ragdolls. */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat")
	float ImpulseScale = 5.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Combat")
	float TraceRadius = 3.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Combat", meta = (ClampMin = 2, ClampMax = 12))
	int32 BladeSamples = 4;

	/** Object types the blade can hit (same idea as ABullet). The default Pawn profile
	 *  ignores the Visibility channel, so an object-type sweep is what actually hits enemies. */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat")
	TArray<TEnumAsByte<EObjectTypeQuery>> HitObjectTypes;

	/** How much a wall/prop bounces the blade back. */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat", meta = (ClampMin = 0, ClampMax = 1))
	float HardSurfaceRestitution = 0.35f;

	/** Fraction of velocity kept after biting into flesh (low = it drags/sticks). */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat", meta = (ClampMin = 0, ClampMax = 1))
	float FleshVelocityKeep = 0.25f;

	/** Once the tip slows below this, a new swing starts and actors can be hit again. */
	UPROPERTY(EditAnywhere, Category = "Melee|Combat")
	float SwingResetSpeed = 120.f;

	UPROPERTY(EditAnywhere, Category = "Melee|Debug")
	bool bDrawDebug = false;

private:
	TWeakObjectPtr<UCameraComponent> Camera;

	FVector HandPos = FVector::ZeroVector;
	FVector HandVel = FVector::ZeroVector;
	FQuat HandRot = FQuat::Identity;
	FVector2D GuardOffset = FVector2D::ZeroVector; // (camera Y, camera Z)
	FVector LastCameraPos = FVector::ZeroVector;

	FVector LocalBase = FVector::ZeroVector;
	FVector LocalTip = FVector(90.f, 0.f, 0.f);

	bool bGuarding = false;
	float TipSpeed = 0.f;

	TArray<FVector> PrevBladePoints;
	TSet<TWeakObjectPtr<AActor>> HitThisSwing;

	// engine shapes, grabbed once in the constructor for AddPart()
	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeShape;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CylinderShape;

	bool bWasFastLastFrame = false;
	float LastWhooshTime = -10.f;
	float LastImpactSoundTime = -10.f;

	void ApplyLook();
	void PlayRandom(const TArray<TSoftObjectPtr<USoundBase>>& Sounds, const FVector& Where, float Volume, float Pitch);

	void CacheBladeLocals();
	void SnapToTarget(const FTransform& CamXf);
	FVector ComputeTargetWorld(const FTransform& CamXf) const;
	FQuat ComputeTargetRotation(const FTransform& CamXf, const FVector& Hand, const FVector& RelVel) const;
	void GatherBladePoints(const FVector& Pos, const FQuat& Rot, TArray<FVector>& Out) const;
	bool SweepBlade(const TArray<FVector>& From, const TArray<FVector>& To, float StepDt);
	void HandleHit(const FHitResult& Hit, const FVector& PointVel);
};
