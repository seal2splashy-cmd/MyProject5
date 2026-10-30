// MeleeWeaponTypes.h
// The starter melee set. Each weapon is just data on top of AMeleeWeapon: a look built
// from engine shapes, where the hitbox runs, and the physics numbers that give it its feel.
// Grip at the origin, weapon along +X, striking edge towards +Z.
//
//   Pipe     1.8 kg  - all-rounder
//   Crowbar  1.4 kg  - quick, hooked tip
//   Machete  0.9 kg  - fast and sharp, bites deep, weak against walls
//   Fire axe 2.8 kg  - slow, laggy, devastating

#pragma once

#include "CoreMinimal.h"
#include "MeleeWeapon.h"
#include "MeleeWeaponTypes.generated.h"

UCLASS()
class MYPROJECT5_API AMeleePipe : public AMeleeWeapon
{
	GENERATED_BODY()
public:
	AMeleePipe();
};

UCLASS()
class MYPROJECT5_API AMeleeCrowbar : public AMeleeWeapon
{
	GENERATED_BODY()
public:
	AMeleeCrowbar();
};

UCLASS()
class MYPROJECT5_API AMeleeMachete : public AMeleeWeapon
{
	GENERATED_BODY()
public:
	AMeleeMachete();
};

UCLASS()
class MYPROJECT5_API AMeleeFireAxe : public AMeleeWeapon
{
	GENERATED_BODY()
public:
	AMeleeFireAxe();
};
