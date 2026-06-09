// Copyright Cieran Nicholas. All Rights Reserved.

#include "Movement/LemonSavedMove.h"
#include "Movement/LemonCharacterMovementComponent.h"
#include "GameFramework/Character.h"

// =================================================================================================
//  FSavedMove_Lemon
// =================================================================================================
FSavedMove_Lemon::FSavedMove_Lemon()
{
	Saved_bWantsToSprint = 0;
	Saved_bWantsToWalk = 0;
	Saved_bWantsToWallRun = 0;
	Saved_CoyoteTime = 0.f;
	Saved_WallRunTime = 0.f;
	Saved_WallRunCooldown = 0.f;
}

void FSavedMove_Lemon::Clear()
{
	Super::Clear();
	Saved_bWantsToSprint = 0;
	Saved_bWantsToWalk = 0;
	Saved_bWantsToWallRun = 0;
	Saved_CoyoteTime = 0.f;
	Saved_WallRunTime = 0.f;
	Saved_WallRunCooldown = 0.f;
}

uint8 FSavedMove_Lemon::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (Saved_bWantsToSprint) { Result |= FLAG_Custom_0; }
	if (Saved_bWantsToWalk) { Result |= FLAG_Custom_1; }
	if (Saved_bWantsToWallRun) { Result |= FLAG_Custom_2; }
	return Result;
}

bool FSavedMove_Lemon::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Lemon* NewLemonMove = static_cast<const FSavedMove_Lemon*>(NewMove.Get());
	if (Saved_bWantsToSprint != NewLemonMove->Saved_bWantsToSprint) { return false; }
	if (Saved_bWantsToWalk != NewLemonMove->Saved_bWantsToWalk) { return false; }
	if (Saved_bWantsToWallRun != NewLemonMove->Saved_bWantsToWallRun) { return false; }
	// Note: Saved_CoyoteTime is deliberately NOT compared here. It accumulates linearly with dt, so a
	// combined move (replayed from the older move's value over the summed dt) reaches the same total; and
	// the base already refuses to combine across a jump-press frame, so a coyote jump is never merged away.
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Lemon::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const ULemonCharacterMovementComponent* CMC = Cast<ULemonCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Saved_bWantsToSprint = CMC->Safe_bWantsToSprint;
		Saved_bWantsToWalk = CMC->Safe_bWantsToWalk;
		Saved_bWantsToWallRun = CMC->Safe_bWantsToWallRun;
		Saved_CoyoteTime = CMC->Safe_CoyoteTime;
		Saved_WallRunTime = CMC->Safe_WallRunTime;
		Saved_WallRunCooldown = CMC->Safe_WallRunCooldown;
	}
}

void FSavedMove_Lemon::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (ULemonCharacterMovementComponent* CMC = Cast<ULemonCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		CMC->Safe_bWantsToSprint = Saved_bWantsToSprint;
		CMC->Safe_bWantsToWalk = Saved_bWantsToWalk;
		CMC->Safe_bWantsToWallRun = Saved_bWantsToWallRun;
		CMC->Safe_CoyoteTime = Saved_CoyoteTime;
		CMC->Safe_WallRunTime = Saved_WallRunTime;
		CMC->Safe_WallRunCooldown = Saved_WallRunCooldown;
	}
}

// =================================================================================================
//  FNetworkPredictionData_Client_Lemon
// =================================================================================================
FNetworkPredictionData_Client_Lemon::FNetworkPredictionData_Client_Lemon(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr FNetworkPredictionData_Client_Lemon::AllocateNewMove()
{
	return MakeShared<FSavedMove_Lemon>();
}
