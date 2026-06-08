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
