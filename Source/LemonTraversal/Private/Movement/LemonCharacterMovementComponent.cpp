// Copyright Cieran Nicholas. All Rights Reserved.

#include "Movement/LemonCharacterMovementComponent.h"
#include "Movement/LemonMovementSet.h"
#include "Movement/LemonSavedMove.h"

ULemonCharacterMovementComponent::ULemonCharacterMovementComponent()
{
	Safe_bWantsToSprint = 0;
	Safe_bWantsToWalk = 0;
	CurrentGait = ELemonGait::Run;

	// Fallback used only when no MovementSet asset is assigned (mirrors ULemonMovementSet's Run defaults).
	DefaultGaitSettings.MaxSpeed = 600.f;
	DefaultGaitSettings.MaxAcceleration = 2048.f;
	DefaultGaitSettings.BrakingDeceleration = 2048.f;
}

void ULemonCharacterMovementComponent::SetSprintHeld(bool bHeld)
{
	Safe_bWantsToSprint = bHeld ? 1 : 0;
}

void ULemonCharacterMovementComponent::SetWalkHeld(bool bHeld)
{
	Safe_bWantsToWalk = bHeld ? 1 : 0;
}

bool ULemonCharacterMovementComponent::ShouldUseGaitTuning() const
{
	return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
}

const FLemonGaitSettings& ULemonCharacterMovementComponent::GetActiveGaitSettings() const
{
	if (MovementSet)
	{
		return MovementSet->GetSettingsForGait(ResolveGait());
	}
	return DefaultGaitSettings;
}

bool ULemonCharacterMovementComponent::CanSprint() const
{
	if (!UpdatedComponent || !IsMovingOnGround())
	{
		return false;
	}

	// Require meaningful, mostly-forward input. This is a design choice — relax the dot threshold
	// (or drop the check) if you want omni-directional sprint.
	if (Acceleration.IsNearlyZero())
	{
		return false;
	}

	const FVector Forward = UpdatedComponent->GetForwardVector();
	const FVector AccelDir = Acceleration.GetSafeNormal();
	return FVector::DotProduct(Forward, AccelDir) > 0.5f; // within ~60 degrees of forward
}

ELemonGait ULemonCharacterMovementComponent::ResolveGait() const
{
	if (Safe_bWantsToSprint && CanSprint())
	{
		return ELemonGait::Sprint;
	}
	if (Safe_bWantsToWalk)
	{
		return ELemonGait::Walk;
	}
	return ELemonGait::Run;
}

float ULemonCharacterMovementComponent::GetMaxSpeed() const
{
	if (ShouldUseGaitTuning())
	{
		return GetActiveGaitSettings().MaxSpeed;
	}
	return Super::GetMaxSpeed();
}

float ULemonCharacterMovementComponent::GetMaxAcceleration() const
{
	if (ShouldUseGaitTuning())
	{
		return GetActiveGaitSettings().MaxAcceleration;
	}
	return Super::GetMaxAcceleration();
}

float ULemonCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	if (ShouldUseGaitTuning())
	{
		return GetActiveGaitSettings().BrakingDeceleration;
	}
	return Super::GetMaxBrakingDeceleration();
}

void ULemonCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	// Server (and replayed client moves) reconstruct intent from the flags packed in GetCompressedFlags().
	Safe_bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	Safe_bWantsToWalk = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

void ULemonCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	// Cache the resolved gait after the move so gameplay/animation can query GetGait().
	CurrentGait = ResolveGait();
}

FNetworkPredictionData_Client* ULemonCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (ClientPredictionData == nullptr)
	{
		ULemonCharacterMovementComponent* MutableThis = const_cast<ULemonCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Lemon(*this);
	}

	return ClientPredictionData;
}
