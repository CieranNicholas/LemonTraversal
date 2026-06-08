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
}

void FSavedMove_Lemon::Clear()
{
	Super::Clear();
	Saved_bWantsToSprint = 0;
	Saved_bWantsToWalk = 0;
}

uint8 FSavedMove_Lemon::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (Saved_bWantsToSprint) { Result |= FLAG_Custom_0; }
	if (Saved_bWantsToWalk) { Result |= FLAG_Custom_1; }
	return Result;
}

bool FSavedMove_Lemon::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Lemon* NewLemonMove = static_cast<const FSavedMove_Lemon*>(NewMove.Get());
	if (Saved_bWantsToSprint != NewLemonMove->Saved_bWantsToSprint) { return false; }
	if (Saved_bWantsToWalk != NewLemonMove->Saved_bWantsToWalk) { return false; }
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Lemon::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const ULemonCharacterMovementComponent* CMC = Cast<ULemonCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		Saved_bWantsToSprint = CMC->Safe_bWantsToSprint;
		Saved_bWantsToWalk = CMC->Safe_bWantsToWalk;
	}
}

void FSavedMove_Lemon::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (ULemonCharacterMovementComponent* CMC = Cast<ULemonCharacterMovementComponent>(C->GetCharacterMovement()))
	{
		CMC->Safe_bWantsToSprint = Saved_bWantsToSprint;
		CMC->Safe_bWantsToWalk = Saved_bWantsToWalk;
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
