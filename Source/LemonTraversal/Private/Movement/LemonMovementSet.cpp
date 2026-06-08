// Copyright Cieran Nicholas. All Rights Reserved.

#include "Movement/LemonMovementSet.h"

ULemonMovementSet::ULemonMovementSet()
{
	// Sensible FPS defaults so a freshly-created asset feels right immediately.
	Walk.MaxSpeed = 200.f;
	Walk.MaxAcceleration = 1024.f;
	Walk.BrakingDeceleration = 2048.f;

	Run.MaxSpeed = 600.f;
	Run.MaxAcceleration = 2048.f;
	Run.BrakingDeceleration = 2048.f;

	Sprint.MaxSpeed = 900.f;
	Sprint.MaxAcceleration = 2560.f;
	Sprint.BrakingDeceleration = 1024.f;
}

const FLemonGaitSettings& ULemonMovementSet::GetSettingsForGait(ELemonGait Gait) const
{
	switch (Gait)
	{
	case ELemonGait::Walk:
		return Walk;
	case ELemonGait::Sprint:
		return Sprint;
	case ELemonGait::Run:
	default:
		return Run;
	}
}
