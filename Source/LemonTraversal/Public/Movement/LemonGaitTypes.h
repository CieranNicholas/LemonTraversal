// Copyright Cieran Nicholas. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LemonGaitTypes.generated.h"

/**
 * Discrete locomotion gaits, ordered slowest -> fastest.
 * Resolved every movement tick from prediction-safe input intent.
 */
UENUM(BlueprintType)
enum class ELemonGait : uint8
{
	Walk	UMETA(DisplayName = "Walk"),
	Run		UMETA(DisplayName = "Run"),
	Sprint	UMETA(DisplayName = "Sprint")
};

/**
 * Tuning for a single gait. Authored in a ULemonMovementSet Data Asset so designers
 * can tweak feel without recompiling. Each field maps to a CMC getter override.
 */
USTRUCT(BlueprintType)
struct FLemonGaitSettings
{
	GENERATED_BODY()

	/** Target ground speed for this gait (cm/s). Feeds GetMaxSpeed(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float MaxSpeed = 600.f;

	/** How hard we accelerate toward MaxSpeed. Feeds GetMaxAcceleration(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float MaxAcceleration = 2048.f;

	/** How hard we brake when there's no input. Feeds GetMaxBrakingDeceleration(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait", meta = (ClampMin = "0"))
	float BrakingDeceleration = 2048.f;
};

/**
 * Custom movement modes for the LemonTraversal CMC. Stored in UCharacterMovementComponent::CustomMovementMode
 * while MovementMode == MOVE_Custom. Unscoped (like the engine's EMovementMode) so it compares directly
 * against the uint8 CustomMovementMode byte.
 */
UENUM(BlueprintType)
enum ELemonCustomMovementMode : uint8
{
	CMOVE_Slide		UMETA(DisplayName = "Slide"),
	CMOVE_WallRun	UMETA(DisplayName = "Wall Run"),
	CMOVE_MAX		UMETA(Hidden)
};

/**
 * Tuning for the slide custom movement mode. Authored in a ULemonMovementSet Data Asset.
 */
USTRUCT(BlueprintType)
struct FLemonSlideSettings
{
	GENERATED_BODY()

	/** Minimum ground speed required to start a slide (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float EnterSpeed = 700.f;

	/** Slide ends when ground speed drops below this (cm/s). Keep below EnterSpeed for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float MinSpeed = 300.f;

	/** One-time forward speed boost applied on slide entry (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float EnterImpulse = 300.f;

	/** Ground friction during a slide. Low keeps momentum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float Friction = 0.4f;

	/** Braking deceleration during a slide (cm/s^2). Low = long slide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float BrakingDeceleration = 600.f;

	/** Downhill acceleration factor — how strongly slopes speed you up / slow you down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float SlopeForce = 1500.f;

	/** How fast the slide direction can be steered left/right, in degrees per second. Keep small for a
	 *  subtle feel — steering only rotates your heading, it never adds speed. 0 = no steering (ballistic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float SteerRate = 70.f;

	/** Upper speed clamp during a slide (cm/s) so momentum isn't capped by walking speeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide", meta = (ClampMin = "0"))
	float MaxSpeed = 1600.f;
};

/**
 * Tuning for the wall-run custom movement mode (CMOVE_WallRun). Authored in a ULemonMovementSet Data Asset.
 * A zero-gravity, horizontal run along vertical walls that you hold into, capped by MaxDuration.
 */
USTRUCT(BlueprintType)
struct FLemonWallRunSettings
{
	GENERATED_BODY()

	/** Minimum horizontal speed to start AND sustain a wall run (cm/s). Drop below this and you detach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float MinSpeed = 500.f;

	/** Upper speed clamp while wall running (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float MaxSpeed = 1400.f;

	/** How far past the capsule radius we sweep sideways to find a wall (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float WallReach = 45.f;

	/** A surface counts as a wall only if |Normal.Z| <= this (0 = perfectly vertical, 1 = any slope). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxWallZ = 0.35f;

	/** How hard you must steer AWAY from the wall to peel off mid-run (dot of input dir vs -WallNormal must
	 *  drop below -this to detach). Engaging is gated by holding jump, not movement — this only governs the
	 *  voluntary "steer off the wall" detach while running. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0", ClampMax = "1"))
	float MinInputIntoWall = 0.35f;

	/** Along-wall acceleration from forward input (cm/s^2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float Acceleration = 2000.f;

	/** Along-wall friction. Low keeps momentum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float Friction = 0.2f;

	/** Gentle velocity pushed into the wall each tick to hold contact (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float StickForce = 200.f;

	/** Max time a single wall run can last before it auto-detaches (s). 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float MaxDuration = 1.5f;

	/** After a wall-jump or timeout, how long before you can re-attach (s). Stops infinite single-wall climbs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float ReattachCooldown = 0.4f;

	/** Wall-jump speed pushed away from the wall, along its normal (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float WallJumpOutSpeed = 520.f;

	/** Wall-jump vertical speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0"))
	float WallJumpUpSpeed = 600.f;

	/** Fraction of along-wall momentum carried into the wall-jump (0 = pure out+up, 1 = keep all forward speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun", meta = (ClampMin = "0", ClampMax = "1"))
	float WallJumpMomentumKeep = 0.5f;
};

/**
 * Tuning for Source-style air strafing while falling. Authored in a ULemonMovementSet Data Asset.
 * Strafe + turn to accelerate in the air; capped so speed can't run away.
 */
USTRUCT(BlueprintType)
struct FLemonAirSettings
{
	GENERATED_BODY()

	/** Master toggle. Off = the engine's default air control (lerp toward input, no speed gain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air")
	bool bEnableAirStrafe = true;

	/** Only the strafe (left/right) input drives air acceleration; forward/back is ignored in the air. Lets
	 *  you keep holding W from a run without it diluting the A/D strafe (Counter-Strike feel). Off = the
	 *  full input direction is used (authentic, but then holding W weakens strafing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air")
	bool bAirStrafeLateralOnly = true;

	/** Air acceleration toward the input direction (cm/s^2). Higher = snappier strafing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air", meta = (ClampMin = "0"))
	float AirAcceleration = 6000.f;

	/** Per-direction "wish speed" cap (cm/s). Keep LOW — this is the trick that makes strafe+turn gain
	 *  speed: you can only accelerate up to this along any single input direction, so curving the input
	 *  (mouse turn + strafe) keeps adding velocity in new directions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air", meta = (ClampMin = "0"))
	float AirSpeedCap = 300.f;

	/** Hard cap on total horizontal air speed (cm/s) so strafing can't run away. Pre-existing momentum
	 *  already above this (e.g. a slide-hop) is preserved but never grown further. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air", meta = (ClampMin = "0"))
	float MaxAirSpeed = 1400.f;
};
