// Copyright Cieran Nicholas. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Movement/LemonGaitTypes.h"
#include "LemonMovementSet.generated.h"

/**
 * Per-gait movement tuning.
 * Create one via: Content Browser -> right-click -> Miscellaneous -> Data Asset -> "LemonMovementSet",
 * then assign it to the character's LemonCharacterMovementComponent.
 */
UCLASS(BlueprintType)
class LEMONTRAVERSAL_API ULemonMovementSet : public UDataAsset
{
	GENERATED_BODY()

public:
	ULemonMovementSet();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FLemonGaitSettings Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FLemonGaitSettings Run;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait")
	FLemonGaitSettings Sprint;

	/** Tuning for the slide custom movement mode (CMOVE_Slide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide")
	FLemonSlideSettings Slide;

	/** Tuning for the wall-run custom movement mode (CMOVE_WallRun). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WallRun")
	FLemonWallRunSettings WallRun;

	/** Tuning for Source-style air strafing while falling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air")
	FLemonAirSettings Air;

	/** Coyote time: grace period (seconds) after walking off a ledge during which a jump still counts as
	 *  a grounded jump. Forgives pressing jump a moment too late. 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "0"))
	float CoyoteTime = 0.15f;

	/** Returns the tuning struct for the requested gait (defaults to Run for safety). */
	const FLemonGaitSettings& GetSettingsForGait(ELemonGait Gait) const;
};
