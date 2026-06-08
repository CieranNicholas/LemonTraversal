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
