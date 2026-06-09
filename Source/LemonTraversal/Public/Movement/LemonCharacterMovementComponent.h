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

	/** Set wall-run intent — wired to the jump button being HELD (call from input on the owning client).
	 *  Holding jump while airborne (from a jump) next to a wall is what attaches you. Prediction-safe. */
	UFUNCTION(BlueprintCallable, Category = "Lemon|Movement")
	void SetWallRunHeld(bool bHeld);

	/** Gait resolved this tick. Valid on the owning client & server; on simulated proxies this stays Run (see notes). */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	ELemonGait GetGait() const { return CurrentGait; }

	/** True while in the custom slide movement mode (MOVE_Custom / CMOVE_Slide). */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	bool IsSliding() const;

	/** True when a jump pressed right now should be forgiven as a grounded jump (coyote time): we left the
	 *  ground by walking off a ledge (not jumping) and are still within the grace window. Read by the
	 *  character's CanJumpInternal/CheckJumpInput overrides. */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	bool CanCoyoteJump() const;

	/** True while in the custom wall-run movement mode (MOVE_Custom / CMOVE_WallRun). */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	bool IsWallRunning() const;

	/** Outward normal of the wall we're running on (zero when not wall running). Cosmetic / camera use. */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	FVector GetWallRunNormal() const { return WallRunNormal; }

	/** Which side the wall is on for camera tilt: +1 = wall on the right, -1 = wall on the left, 0 = none. */
	UFUNCTION(BlueprintPure, Category = "Lemon|Movement")
	float GetWallRunSide() const;

	//~ Begin UCharacterMovementComponent interface
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
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

	/** Coyote-time grace window (seconds), from the MovementSet or the built-in fallback. */
	float GetCoyoteTime() const;

	// --- Wall run ---
	/** Per-frame physics for CMOVE_WallRun (zero-G along a wall, modeled on the engine's PhysFlying). */
	void PhysWallRun(float DeltaTime, int32 Iterations);

	/** Sweep left and right for a near-vertical wall. Returns the chosen wall's outward normal + hit. */
	bool DetectWall(FVector& OutNormal, FHitResult& OutHit) const;

	/** True if a wall run may begin/continue right now (airborne, fast enough, wall found, holding into it). */
	bool CanWallRun(FVector& OutNormal, FHitResult& OutHit) const;

	/** Apply wall-run entry effects (zero vertical velocity, align heading along the wall, reset the timer).
	 *  Uses the WallRunNormal cached by the entry check immediately before the mode change. */
	void EnterWallRun();

	/** Tear-down when leaving the wall-run mode. */
	void ExitWallRun();

	/** Active wall-run tuning, falling back to built-in defaults when no MovementSet is assigned. */
	const FLemonWallRunSettings& GetWallRunSettings() const;

	// --- Prediction-safe intent --------------------------------------------------------------
	// Mirrored into FSavedMove_Lemon and packed into compressed flags. The "Safe_" prefix marks
	// state that must survive client move replay during server correction.
	uint8 Safe_bWantsToSprint : 1;
	uint8 Safe_bWantsToWalk : 1;
	/** Jump button held — the wall-run engage intent (packed into FLAG_Custom_2). */
	uint8 Safe_bWantsToWallRun : 1;

	/** Seconds airborne since we last left the ground (reset on ground / slide, accumulated while falling).
	 *  Derived state, but still prediction-safe: it's mirrored into FSavedMove_Lemon so a coyote jump
	 *  replays identically after a server correction. The first predicted *non-boolean* state in the CMC. */
	float Safe_CoyoteTime;

	/** Seconds spent in the current wall run (drives the zero-G MaxDuration timeout). Predicted via
	 *  FSavedMove_Lemon, same as Safe_CoyoteTime. */
	float Safe_WallRunTime;

	/** Counts down after a wall-jump / timeout; while > 0 a new wall run can't begin (stops single-wall
	 *  climbing). Predicted via FSavedMove_Lemon. */
	float Safe_WallRunCooldown;

	/** Outward normal of the wall currently being run on. Derived each tick from a sweep — NOT predicted
	 *  state, just cached for the wall-jump impulse and the cosmetic camera tilt. */
	FVector WallRunNormal;

	/** Gait resolved each movement tick. Derived state — intentionally not replicated. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Lemon|Movement")
	ELemonGait CurrentGait;

private:
	/** Fallback tuning used when MovementSet is null, so the component still works out-of-the-box. */
	FLemonGaitSettings DefaultGaitSettings;
	FLemonSlideSettings DefaultSlideSettings;
	FLemonWallRunSettings DefaultWallRunSettings;
	float DefaultCoyoteTime = 0.15f;
};
