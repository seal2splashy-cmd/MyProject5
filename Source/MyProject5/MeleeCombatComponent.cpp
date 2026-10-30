// MeleeCombatComponent.cpp

#include "MeleeCombatComponent.h"

#include "MeleeWeapon.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"

UMeleeCombatComponent::UMeleeCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // the weapon ticks itself
}

void UMeleeCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	Camera = FindOwnerCamera();
	if (!Camera.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeCombatComponent on %s: no camera found, melee disabled."), *GetNameSafe(GetOwner()));
		return;
	}

	if (WeaponClass)
	{
		SpawnWeapon(WeaponClass);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeCombatComponent on %s: WeaponClass not set."), *GetNameSafe(GetOwner()));
	}
}

void UMeleeCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Weapon)
	{
		Weapon->Destroy();
		Weapon = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

UCameraComponent* UMeleeCombatComponent::FindOwnerCamera() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	TArray<UCameraComponent*> Cameras;
	OwnerActor->GetComponents(Cameras);

	if (CameraComponentName != NAME_None)
	{
		for (UCameraComponent* Cam : Cameras)
		{
			if (Cam && Cam->GetFName() == CameraComponentName)
			{
				return Cam;
			}
		}
	}

	return Cameras.Num() > 0 ? Cameras[0] : nullptr;
}

void UMeleeCombatComponent::SpawnWeapon(TSubclassOf<AMeleeWeapon> ClassToSpawn)
{
	UWorld* World = GetWorld();
	UCameraComponent* Cam = Camera.Get();
	if (!World || !Cam || !ClassToSpawn)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Weapon = World->SpawnActor<AMeleeWeapon>(ClassToSpawn, Cam->GetComponentTransform(), Params);
	if (Weapon)
	{
		Weapon->InitialiseFor(Cam);
	}
}

void UMeleeCombatComponent::EquipWeapon(TSubclassOf<AMeleeWeapon> NewWeaponClass)
{
	if (Weapon)
	{
		Weapon->Destroy();
		Weapon = nullptr;
	}
	WeaponClass = NewWeaponClass;
	SpawnWeapon(WeaponClass);
}

void UMeleeCombatComponent::BindInput(UInputComponent* PlayerInputComponent)
{
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("MeleeCombatComponent: input component isn't Enhanced Input, melee input not bound."));
		return;
	}

	if (GuardAction)
	{
		EIC->BindAction(GuardAction, ETriggerEvent::Started, this, &UMeleeCombatComponent::OnGuardStarted);
		EIC->BindAction(GuardAction, ETriggerEvent::Completed, this, &UMeleeCombatComponent::OnGuardEnded);
		EIC->BindAction(GuardAction, ETriggerEvent::Canceled, this, &UMeleeCombatComponent::OnGuardEnded);
	}

	if (ThrustAction)
	{
		EIC->BindAction(ThrustAction, ETriggerEvent::Started, this, &UMeleeCombatComponent::OnThrust);
	}
}

FVector2D UMeleeCombatComponent::FilterLook(const FVector2D& RawLookDelta)
{
	if (!Weapon)
	{
		return RawLookDelta;
	}

	Weapon->AddMouseInput(RawLookDelta);
	return RawLookDelta * Weapon->GetLookScale();
}

void UMeleeCombatComponent::OnGuardStarted(const FInputActionValue& /*Value*/)
{
	if (Weapon)
	{
		Weapon->BeginGuard();
	}
}

void UMeleeCombatComponent::OnGuardEnded(const FInputActionValue& /*Value*/)
{
	if (Weapon)
	{
		Weapon->EndGuard();
	}
}

void UMeleeCombatComponent::OnThrust(const FInputActionValue& /*Value*/)
{
	if (Weapon)
	{
		Weapon->Thrust();
	}
}
