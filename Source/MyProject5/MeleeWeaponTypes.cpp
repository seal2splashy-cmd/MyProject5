// MeleeWeaponTypes.cpp

#include "MeleeWeaponTypes.h"

namespace MeleeLook
{
	const TCHAR* RustMat = TEXT("/Game/Generated/Materials/M_RustMetal.M_RustMetal");
	const TCHAR* WoodMat = TEXT("/Game/Generated/Materials/M_Wood.M_Wood");

	const FLinearColor Steel(0.55f, 0.56f, 0.58f);
	const FLinearColor DarkSteel(0.18f, 0.18f, 0.2f);
	const FLinearColor Rust(0.35f, 0.18f, 0.08f);
	const FLinearColor Wood(0.32f, 0.19f, 0.09f);
	const FLinearColor Rubber(0.03f, 0.03f, 0.03f);
	const FLinearColor FireRed(0.6f, 0.04f, 0.03f);

	// Engine cylinder: 100 cm tall along its local Z, 100 cm across, centred.
	// Engine cube: 100 cm, centred. Pitch -90 lays a cylinder along +X.
	const FRotator AlongX(-90.f, 0.f, 0.f);

	FVector Rod(float LengthCm, float DiameterCm)
	{
		return FVector(DiameterCm / 100.f, DiameterCm / 100.f, LengthCm / 100.f);
	}
	FVector Box(float X, float Y, float Z)
	{
		return FVector(X / 100.f, Y / 100.f, Z / 100.f);
	}
}

// ------------------------------------------------------------------ pipe

AMeleePipe::AMeleePipe()
{
	using namespace MeleeLook;

	DisplayName = TEXT("Pipe");

	SetMainPart(false, FVector(45.f, 0.f, 0.f), AlongX, Rod(90.f, 4.f), Rust, RustMat);
	AddPart(TEXT("Tape"), false, FVector(9.f, 0.f, 0.f), AlongX, Rod(18.f, 4.4f), Rubber);
	AddPart(TEXT("Elbow"), false, FVector(90.f, 0.f, 0.f), AlongX, Rod(4.f, 5.4f), DarkSteel);

	SetBladePoints(FVector(25.f, 0.f, 0.f), FVector(92.f, 0.f, 0.f));

	MassKg = 1.8f;
	Stiffness = 950.f;
	DampingRatio = 0.55f;
	DamagePerMomentum = 0.03f;
	HardSurfaceRestitution = 0.4f;
	FleshVelocityKeep = 0.3f;
}

// ------------------------------------------------------------------ crowbar

AMeleeCrowbar::AMeleeCrowbar()
{
	using namespace MeleeLook;

	DisplayName = TEXT("Crowbar");

	SetMainPart(false, FVector(37.5f, 0.f, 0.f), AlongX, Rod(75.f, 2.6f), DarkSteel, RustMat);
	AddPart(TEXT("Grip"), false, FVector(8.f, 0.f, 0.f), AlongX, Rod(16.f, 3.2f), Rubber);
	// Hook: 14 cm, angled 60 degrees up from the shaft towards +Z
	// (cylinder axis = (-sin p, 0, cos p); p = -30 gives (0.5, 0, 0.87))
	AddPart(TEXT("Hook"), false, FVector(78.5f, 0.f, 6.f), FRotator(-30.f, 0.f, 0.f), Rod(14.f, 2.6f), DarkSteel, RustMat);
	// Flattened claw at the other end
	AddPart(TEXT("Claw"), true, FVector(-1.5f, 0.f, -1.f), FRotator(20.f, 0.f, 0.f), Box(5.f, 3.2f, 1.f), DarkSteel);

	SetBladePoints(FVector(18.f, 0.f, 0.f), FVector(84.f, 0.f, 11.f));

	MassKg = 1.4f;
	Stiffness = 850.f;
	DampingRatio = 0.5f;
	DamagePerMomentum = 0.034f;
	HardSurfaceRestitution = 0.45f;
	FleshVelocityKeep = 0.35f;
}

// ------------------------------------------------------------------ machete

AMeleeMachete::AMeleeMachete()
{
	using namespace MeleeLook;

	DisplayName = TEXT("Machete");

	// Handle is the main part, blade is a thin box with its edge facing +Z
	SetMainPart(false, FVector(7.f, 0.f, 0.f), AlongX, Rod(14.f, 3.2f), Wood, WoodMat);
	AddPart(TEXT("Guard"), true, FVector(14.5f, 0.f, 0.5f), FRotator::ZeroRotator, Box(1.2f, 3.6f, 6.5f), DarkSteel);
	AddPart(TEXT("Blade"), true, FVector(40.f, 0.f, 1.f), FRotator::ZeroRotator, Box(50.f, 0.35f, 6.f), Steel);
	// Clipped tip: a small box rotated 45 degrees to fake the curve
	AddPart(TEXT("Tip"), true, FVector(65.f, 0.f, 2.f), FRotator(45.f, 0.f, 0.f), Box(4.2f, 0.35f, 4.2f), Steel);

	SetBladePoints(FVector(17.f, 0.f, 3.f), FVector(67.f, 0.f, 3.f));

	MassKg = 0.9f;
	Stiffness = 700.f;
	DampingRatio = 0.6f;
	MinDamageSpeed = 220.f;
	DamagePerMomentum = 0.055f;      // sharp: light but it cuts
	HardSurfaceRestitution = 0.25f;
	FleshVelocityKeep = 0.15f;       // bites and sticks
	TraceRadius = 2.f;
}

// ------------------------------------------------------------------ fire axe

AMeleeFireAxe::AMeleeFireAxe()
{
	using namespace MeleeLook;

	DisplayName = TEXT("Fire Axe");

	SetMainPart(false, FVector(42.5f, 0.f, 0.f), AlongX, Rod(85.f, 3.6f), Wood, WoodMat);
	// Head straddles the shaft end; the blade side points +Z, the spike points -Z
	AddPart(TEXT("Head"), true, FVector(80.f, 0.f, 3.f), FRotator::ZeroRotator, Box(7.f, 3.2f, 14.f), FireRed);
	AddPart(TEXT("Edge"), true, FVector(80.f, 0.f, 12.f), FRotator::ZeroRotator, Box(9.5f, 1.2f, 4.f), Steel);
	AddPart(TEXT("Spike"), true, FVector(80.f, 0.f, -7.f), FRotator(0.f, 0.f, 0.f), Box(3.f, 2.2f, 7.f), FireRed);

	SetBladePoints(FVector(60.f, 0.f, 4.f), FVector(82.f, 0.f, 14.f));

	MassKg = 2.8f;
	Stiffness = 1300.f;
	DampingRatio = 0.48f;
	MinDamageSpeed = 200.f;
	DamagePerMomentum = 0.04f;
	HardSurfaceRestitution = 0.3f;
	FleshVelocityKeep = 0.12f;
	ThrustImpulse = 1200.f;          // not a stabbing tool
	SoundVolume = 1.1f;
}
