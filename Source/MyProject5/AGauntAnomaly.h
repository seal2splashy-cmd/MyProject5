// AGauntAnomaly.h
// The forest anomaly: the tall, gaunt creature (Assets/Anomaly/Gaunt). Same brain as
// ASmartEnemy (FSM + Q-learning); this class only adds the body.
//
// Two ways it can look, picked automatically at runtime and in the editor:
//  - Rigged (after Mixamo, see Docs/GAUNT_ANOMALY.md): skeletal mesh SK_AH_Gaunt with
//    Idle/Walk/Run/Attack/Death clips chosen from the AI state and speed, ragdoll on death.
//  - Stand-in (until then): the static mesh SM_AH_Gaunt, given life procedurally - stalking
//    lean, stride bob and sway, nervous twitches, a lunge when it attacks, a topple when it dies.
//
// While it's alive and near the player it breaks up the bodycam picture.
//
// How it hunts (on top of the Q-learning chase):
//  - Stalking: further than StalkDistance it only moves while you aren't looking at it. Look at it and
//    it freezes, clicking at you; look away and it closes in faster than it walks.
//  - Light: the torch beam on its head up close makes it shriek and back off into the dark for a
//    couple of seconds (then it won't flinch again for a while - you can't just pin it with the torch).
//  - Grab: a hit that would kill you is a grab instead - your view is wrenched round to its face, the
//    bodycam breaks up, it screams, and then it finishes you.
//  - Voice: wet breathing you can hear through walls (louder when it's hunting), clicks, a growl when it
//    spots you. Footsteps and NavMesh paths (catwalks and stairs included) come from AEnemy.

#pragma once

#include "CoreMinimal.h"
#include "ASmartEnemy.h"
#include "AGauntAnomaly.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class USkeletalMesh;
class UAnimationAsset;
class UAudioComponent;

UCLASS()
class MYPROJECT5_API AGauntAnomaly : public ASmartEnemy
{
    GENERATED_BODY()

public:
    AGauntAnomaly();

    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    bool IsRigged() const { return bRigged; }

protected:
    virtual void BeginPlay() override;
    virtual void PerformAttack() override;
    virtual void OnDeath() override;
    virtual void HandleState_Chase(float DeltaTime) override;

    // ---- Hunting

    /** Beyond this it stalks: frozen while you look at it, closing in while you don't (cm). */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting")
    float StalkDistance = 1100.f;

    /** How directly you have to look at it to freeze it (cosine; 0.88 = within ~28 degrees). */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting", meta = (ClampMin = 0, ClampMax = 1))
    float LookedAtCosine = 0.88f;

    /** Speed x this while it creeps up on you unseen. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting")
    float StalkSpeedScale = 1.35f;

    /** The torch beam on its head within this range makes it flinch and back off (cm). */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting")
    float TorchFlinchDistance = 1400.f;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting")
    float FlinchDuration = 2.f;

    /** After a flinch the light doesn't stop it again for this long. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting")
    float FlinchCooldown = 6.f;

    /** A killing blow is a grab first: this long with your view held on its face. 0 = just hit. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Hunting", meta = (ClampMin = 0))
    float GrabDuration = 1.8f;

    // ---- Voice

    /** How far its breathing carries (cm). */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Voice")
    float BreathRange = 1300.f;

    /** Seconds between clicks (random in this range); quicker while it's watching you. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Voice")
    FVector2D ClickInterval = FVector2D(1.5f, 4.5f);

    // ---- Assets (made by Tools/import_gaunt_ue58.py)

    UPROPERTY(VisibleAnywhere, Category = "Gaunt")
    UStaticMeshComponent* StandIn;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    TSoftObjectPtr<UStaticMesh> StandInMesh;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    TSoftObjectPtr<USkeletalMesh> RiggedMesh;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    TSoftObjectPtr<UAnimationAsset> IdleAnim;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    TSoftObjectPtr<UAnimationAsset> WalkAnim;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    TSoftObjectPtr<UAnimationAsset> RunAnim;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    TSoftObjectPtr<UAnimationAsset> AttackAnim;

    /** Turns the model to face the actor's forward (+X). Models from Blender/Mixamo face +Y. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    float MeshYaw = -90.f;

    /** Above this speed (cm/s) the rigged version runs instead of walking. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Assets")
    float RunSpeedThreshold = 260.f;

    // ---- Stand-in motion

    /** Forward lean at full speed (degrees). */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Stand-in")
    float MaxLean = 14.f;

    /** Ground covered per stride (cm); sets the bob/sway rhythm. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Stand-in")
    float StrideLength = 110.f;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Stand-in")
    float BobHeight = 3.f;

    UPROPERTY(EditAnywhere, Category = "Gaunt|Stand-in")
    float SwayDegrees = 5.f;

    /** Seconds between twitches (random within this range); halved while hunting the player. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Stand-in")
    FVector2D TwitchInterval = FVector2D(2.5f, 7.f);

    UPROPERTY(EditAnywhere, Category = "Gaunt|Stand-in")
    float TwitchDegrees = 9.f;

    // ---- Bodycam interference

    /** Interference starts at this distance (cm)... */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Bodycam")
    float InterferenceStartDistance = 1800.f;

    /** ...and is full at this distance. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Bodycam")
    float InterferenceFullDistance = 250.f;

    /** Extra interference while it's chasing or attacking you. */
    UPROPERTY(EditAnywhere, Category = "Gaunt|Bodycam")
    float HuntingInterferenceBoost = 0.2f;

private:
    void ApplyVisuals();
    void UpdateRiggedAnimation();
    void UpdateStandIn(float DeltaTime);
    void UpdateInterference();
    void UpdateVoice(float DeltaTime);
    void UpdateGrab();
    void Vocalise(const TCHAR* Prefix, int32 Variants, float Volume, float Range);

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> BreathAudio;

    bool bGrabbing = false;
    float GrabEndTime = 0.f;
    float FlinchUntil = -1.f;
    float FlinchCooldownUntil = 0.f;
    bool bStalkFrozen = false;
    float NextClickTime = 0.f;
    EAIState PrevAIState = EAIState::Patrol;

    bool bRigged = false;

    UPROPERTY(Transient)
    TObjectPtr<UAnimationAsset> LoadedIdle;
    UPROPERTY(Transient)
    TObjectPtr<UAnimationAsset> LoadedWalk;
    UPROPERTY(Transient)
    TObjectPtr<UAnimationAsset> LoadedRun;
    UPROPERTY(Transient)
    TObjectPtr<UAnimationAsset> LoadedAttack;
    UPROPERTY(Transient)
    TObjectPtr<UAnimationAsset> CurrentAnim;

    float AttackAnimUntil = 0.f;

    // stand-in state
    float StridePhase = 0.f;
    float SmoothedSpeed = 0.f;
    float NextTwitchTime = 0.f;
    FRotator TwitchRot = FRotator::ZeroRotator;
    float Lunge = 0.f;            // 1 at the start of an attack, decays
    float FallAlpha = -1.f;       // <0 alive; 0..1 toppling over when dead
    float FallSign = 1.f;         // fall forwards (+1) or backwards (-1)
    float FallSide = 0.f;
};
