// MeleeCombatComponent.h
// Drop-in glue between your pawn and AMeleeWeapon.
//
// Add this component to AMyFPSPawn and it will:
//   - spawn the melee weapon on BeginPlay and hook it to the pawn's camera
//   - bind Guard (hold) and Thrust (press) to Enhanced Input actions
//   - give you FilterLook(), which feeds the mouse into the blade and returns
//     the (possibly reduced) delta your pawn should use to turn the camera
//
// Pawn changes needed (3 lines total):
//   1. Header:          UPROPERTY(VisibleAnywhere) TObjectPtr<UMeleeCombatComponent> Melee;
//   2. Constructor:     Melee = CreateDefaultSubobject<UMeleeCombatComponent>(TEXT("Melee"));
//   3. SetupPlayerInputComponent:  Melee->BindInput(PlayerInputComponent);
//   4. In your Look handler, before turning:  LookDelta = Melee->FilterLook(LookDelta);

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

	/** Call from the pawn's SetupPlayerInputComponent. Safe to call before or after BeginPlay. */
	void BindInput(UInputComponent* PlayerInputComponent);

	/**
	 * Pass the raw mouse/stick look delta through this BEFORE you apply yaw/pitch.
	 * It drives the blade while guarding and returns what's left for the camera.
	 */
	FVector2D FilterLook(const FVector2D& RawLookDelta);

	UFUNCTION(BlueprintPure, Category = "Melee")
	AMeleeWeapon* GetWeapon() const { return Weapon; }

	/** Swap weapons at runtime (pipe -> axe etc). Destroys the current one. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void EquipWeapon(TSubclassOf<AMeleeWeapon> NewWeaponClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Make a Blueprint child of AMeleeWeapon (give it a mesh, place BladeBase/BladeTip) and set it here. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	TSubclassOf<AMeleeWeapon> WeaponClass;

	/** Bool action, e.g. Right Mouse. Held = guard / steer the blade with the mouse. */
	UPROPERTY(EditAnywhere, Category = "Melee|Input")
	TObjectPtr<UInputAction> GuardAction;

	/** Bool action, e.g. Left Mouse. Pressed = stab forward. */
	UPROPERTY(EditAnywhere, Category = "Melee|Input")
	TObjectPtr<UInputAction> ThrustAction;

	/** Leave empty to use the first camera found on the owner. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	FName CameraComponentName = NAME_None;

private:
	UPROPERTY(Transient)
	TObjectPtr<AMeleeWeapon> Weapon;

	TWeakObjectPtr<UCameraComponent> Camera;

	UCameraComponent* FindOwnerCamera() const;
	void SpawnWeapon(TSubclassOf<AMeleeWeapon> ClassToSpawn);

	void OnGuardStarted(const FInputActionValue& Value);
	void OnGuardEnded(const FInputActionValue& Value);
	void OnThrust(const FInputActionValue& Value);
};
