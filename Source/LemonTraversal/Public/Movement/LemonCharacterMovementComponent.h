// Copyright Cieran Nicholas. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Movement/LemonGaitTypes.h"
#include "LemonCharacterMovementComponent.generated.h"

class ULemonMovementSet;

UCLASS(ClassGroup = (LemonTraversal), meta = (BlueprintSpawnableComponent))
class LEMONTRAVERSAL_API ULemonCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_Lemon;

public:
	ULemonCharacterMovementComponent();

	/** Per-gait tuning (speeds / accel / braking). Assign a Data Asset to make this designer-editable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lemon|Movement")
	TObjectPtr<ULemonMovementSet> MovementSet;

	/** Set sprint intent (call from input on the owning client). Prediction-safe. */
	UFUNCTION(BlueprintCallable, Category = "Lemon|Movement")
	void SetSprintHeld(bool bHeld);

	/** Set walk intent (call from input on the owning client). Prediction-safe. */
	UFUNCTION(BlueprintCallable, Category = "Lemon|Movement")
	void SetWalkHeld(bool bHeld);

	/** Gait resolved this tick. Valid on the owning client & server; on simulated proxies this stays Run (see notes). */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	ELemonGait GetGait() const { return CurrentGait; }

	/** True while in the custom slide movement mode (MOVE_Custom / CMOVE_Slide). */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	bool IsSliding() const;

	//~ Begin UCharacterMovementComponent interface
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual bool CanAttemptJump() const override;
	//~ End UCharacterMovementComponent interface

protected:
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual bool CanCrouchInCurrentState() const override;

	/** Resolve the active gait from intent + movement state. Deterministic given prediction-safe inputs. */
	ELemonGait ResolveGait() const;

	/** Whether sprint is currently permitted (grounded + sufficiently forward input). Tweak for your game's feel. */
	bool CanSprint() const;

	/** Active gait tuning, falling back to built-in defaults when no MovementSet is assigned. */
	const FLemonGaitSettings& GetActiveGaitSettings() const;

	/** Movement modes whose speed/accel/braking are governed by gait tuning. */
	bool ShouldUseGaitTuning() const;

	// --- Slide ---
	/** Per-frame physics for CMOVE_Slide (modeled on the engine's PhysWalking). */
	void PhysSlide(float DeltaTime, int32 Iterations);

	/** True if a slide may begin right now (grounded, walkable floor, fast enough). */
	bool CanSlide() const;

	/** Apply slide-entry effects (one-time momentum boost). Capsule is lowered by the base crouch. */
	void EnterSlide();

	/** Tear-down when leaving the slide mode. */
	void ExitSlide();

	/** Active slide tuning, falling back to built-in defaults when no MovementSet is assigned. */
	const FLemonSlideSettings& GetSlideSettings() const;

	// --- Prediction-safe intent --------------------------------------------------------------
	// Mirrored into FSavedMove_Lemon and packed into compressed flags. The "Safe_" prefix marks
	// state that must survive client move replay during server correction.
	uint8 Safe_bWantsToSprint : 1;
	uint8 Safe_bWantsToWalk : 1;

	/** Gait resolved each movement tick. Derived state — intentionally not replicated. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Lemon|Movement")
	ELemonGait CurrentGait;

private:
	/** Fallback tuning used when MovementSet is null, so the component still works out-of-the-box. */
	FLemonGaitSettings DefaultGaitSettings;
	FLemonSlideSettings DefaultSlideSettings;
};
