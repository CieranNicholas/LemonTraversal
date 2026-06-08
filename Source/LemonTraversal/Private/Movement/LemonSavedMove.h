// Copyright Cieran Nicholas. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

/**
 * One recorded move on the owning client.
 *
 * Carries LemonTraversal's prediction-safe intent so that, when the server sends a correction, the
 * client can replay the same inputs and arrive at the same result. This is an implementation detail
 * of ULemonCharacterMovementComponent and deliberately lives in Private — nothing outside the module
 * should depend on it.
 *
 * Adding a new predicted feature (slide, mantle, ...) means adding its Saved_* fields here and
 * threading them through Clear / GetCompressedFlags / CanCombineWith / SetMoveFor / PrepMoveFor.
 */
class FSavedMove_Lemon : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	/** Snapshot of the CMC's prediction-safe intent at the time this move was made. */
	uint8 Saved_bWantsToSprint : 1;
	uint8 Saved_bWantsToWalk : 1;

	FSavedMove_Lemon();

	//~ Begin FSavedMove_Character interface
	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	//~ End FSavedMove_Character interface
};

/**
 * Hands the engine our custom saved-move type for this component's client prediction.
 */
class FNetworkPredictionData_Client_Lemon : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNetworkPredictionData_Client_Lemon(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};
