// MeleeCombatComponent.h
// Glue between AMyFPSPawn and AMeleeWeapon.
//
// - Spawns the melee weapon on BeginPlay (holstered/hidden unless bStartActive)
// - FilterLook(): feed the raw look delta through this BEFORE turning the camera.
//   While guarding it steals most of the mouse to steer the blade.
// - StartGuard / StopGuard / DoThrust: call from your own input handlers
//   (MyProject5 uses legacy BindKey input, so the pawn drives these directly).
// - BindInput(): optional Enhanced Input route if you move to Input Actions later.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MeleeCombatComponent.generated.h"

class AMeleeWeapon;
class UCameraComponent;
class UInputAction;
class UInputComponent;
struct FInputActionValue;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class MYPROJECT5_API UMeleeCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeCombatComponent();

	/** Optional: bind GuardAction/ThrustAction through Enhanced Input. */
	void BindInput(UInputComponent* PlayerInputComponent);

	/** Pass the raw look delta through this before applying yaw/pitch. */
	FVector2D FilterLook(const FVector2D& RawLookDelta);

	void StartGuard();
	void StopGuard();
	void DoThrust();

	/** Show/hide the weapon. Hidden = no sweeps, no damage, mouse untouched. */
	void SetMeleeActive(bool bActive);
	bool IsMeleeActive() const { return bMeleeActive; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	AMeleeWeapon* GetWeapon() const { return Weapon; }

	/** Swap weapons at runtime (pipe -> axe etc). Destroys the current one. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void EquipWeapon(TSubclassOf<AMeleeWeapon> NewWeaponClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Defaults to the plain AMeleeWeapon (a pipe). Point it at a Blueprint child for real meshes. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	TSubclassOf<AMeleeWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, Category = "Melee")
	bool bStartActive = false;

	UPROPERTY(EditAnywhere, Category = "Melee|Input")
	TObjectPtr<UInputAction> GuardAction;

	UPROPERTY(EditAnywhere, Category = "Melee|Input")
	TObjectPtr<UInputAction> ThrustAction;

	/** Leave empty to use the first camera found on the owner. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	FName CameraComponentName = NAME_None;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMeleeWeapon> Weapon;

	TWeakObjectPtr<UCameraComponent> Camera;
	bool bMeleeActive = false;

	UCameraComponent* FindOwnerCamera() const;
	void SpawnWeapon(TSubclassOf<AMeleeWeapon> ClassToSpawn);

	void OnGuardStarted(const FInputActionValue& Value);
	void OnGuardEnded(const FInputActionValue& Value);
	void OnThrust(const FInputActionValue& Value);
};
