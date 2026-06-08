// Copyright Cieran Nicholas. All Rights Reserved.

#include "Movement/LemonCharacterMovementComponent.h"
#include "Movement/LemonMovementSet.h"
#include "Movement/LemonSavedMove.h"
#include "GameFramework/Character.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<bool> CVarLemonSlideDebug(
	TEXT("lemon.SlideDebug"),
	false,
	TEXT("Draw on-screen LemonTraversal movement debug for the locally-controlled character."));
#endif

ULemonCharacterMovementComponent::ULemonCharacterMovementComponent()
{
	Safe_bWantsToSprint = 0;
	Safe_bWantsToWalk = 0;
	Safe_CoyoteTime = 0.f;
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
	if (IsSliding())
	{
		return GetSlideSettings().MaxSpeed;
	}
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
	if (IsSliding())
	{
		return GetSlideSettings().BrakingDeceleration;
	}
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

#if !UE_BUILD_SHIPPING
	if (CVarLemonSlideDebug.GetValueOnGameThread() && GEngine && CharacterOwner && CharacterOwner->IsLocallyControlled())
	{
		const TCHAR* ModeStr = IsSliding() ? TEXT("SLIDE")
			: (IsMovingOnGround() ? TEXT("Ground") : (IsFalling() ? TEXT("Falling") : TEXT("Other")));
		GEngine->AddOnScreenDebugMessage(0x1E33, 0.f, FColor::Cyan,
			FString::Printf(TEXT("Lemon | %s | Spd %.0f (2D %.0f) | Floor %d | WantsCrouch %d | Coyote %.2f/%.2f%s"),
				ModeStr, Velocity.Size(), Velocity.Size2D(), CurrentFloor.IsWalkableFloor() ? 1 : 0, (int32)bWantsToCrouch,
				Safe_CoyoteTime, GetCoyoteTime(), CanCoyoteJump() ? TEXT(" [JUMP OK]") : TEXT("")));
	}
#endif
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

/*
	* Slide (CMOVE_Slide)
	* A custom movement mode. Triggered by the engine's built-in crouch intent (bWantsToCrouch) while
	* movement speed is past the configured threshold, so it needs NO new predicted state.
*/

bool ULemonCharacterMovementComponent::IsSliding() const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == CMOVE_Slide);
}

const FLemonSlideSettings& ULemonCharacterMovementComponent::GetSlideSettings() const
{
	if (MovementSet)
	{
		return MovementSet->Slide;
	}
	return DefaultSlideSettings;
}

float ULemonCharacterMovementComponent::GetCoyoteTime() const
{
	return MovementSet ? MovementSet->CoyoteTime : DefaultCoyoteTime;
}

bool ULemonCharacterMovementComponent::CanCoyoteJump() const
{
	// Only forgive a jump while we're airborne, and only if we got here by walking off a ledge. A real
	// jump bumps JumpCurrentCountPreJump above 0 (set at the top of CheckJumpInput, before the falling
	// pre-increment), so this cleanly excludes mid-air / double jumps from being re-forgiven.
	if (!CharacterOwner || !IsFalling() || CharacterOwner->JumpCurrentCountPreJump > 0)
	{
		return false;
	}

	const float Window = GetCoyoteTime();
	return Window > 0.f && Safe_CoyoteTime <= Window;
}

bool ULemonCharacterMovementComponent::CanSlide() const
{
	if (!UpdatedComponent || !IsMovingOnGround())
	{
		return false;
	}

	// Enough speed to be worth sliding (3D speed so a downhill descent counts fully).
	if (Velocity.SizeSquared() < FMath::Square(GetSlideSettings().EnterSpeed))
	{
		return false;
	}

	return CurrentFloor.IsWalkableFloor();
}

void ULemonCharacterMovementComponent::EnterSlide()
{
	const FLemonSlideSettings& Slide = GetSlideSettings();

	// One-time forward boost along the current planar velocity.
	Velocity += Velocity.GetSafeNormal2D() * Slide.EnterImpulse;

	// Clamp so chained entries can't stack absurd speed.
	if (Velocity.SizeSquared() > FMath::Square(Slide.MaxSpeed))
	{
		Velocity = Velocity.GetSafeNormal() * Slide.MaxSpeed;
	}
}

void ULemonCharacterMovementComponent::ExitSlide()
{
	// Nothing to tear down yet — hook point for future slide camera / animation state.
}

bool ULemonCharacterMovementComponent::CanCrouchInCurrentState() const
{
	// Keep the lowered capsule valid during the slide; the base would otherwise un-crouch in MOVE_Custom
	// (CanCrouchInCurrentState requires IsMovingOnGround(), which excludes custom modes).
	if (IsSliding())
	{
		return CanEverCrouch();
	}
	return Super::CanCrouchInCurrentState();
}

bool ULemonCharacterMovementComponent::CanAttemptJump() const
{
	// Allow jumping out of a slide (slide-hop). The base blocks it because bWantsToCrouch is set.
	if (IsSliding())
	{
		return IsJumpAllowed();
	}
	return Super::CanAttemptJump();
}

void ULemonCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Base applies crouch / un-crouch from bWantsToCrouch (lowers the capsule).
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Coyote time: tally how long we've been airborne since leaving the ground, so a jump pressed within
	// the grace window still counts as a grounded jump (see CanCoyoteJump). Runs before CheckJumpInput,
	// on both the owning client and the server, accumulating the same per-move dt either side. Ground and
	// slide both count as "on the ground" for jump purposes, so we reset there and only tally while falling.
	if (IsFalling())
	{
		Safe_CoyoteTime += DeltaSeconds;
	}
	else
	{
		Safe_CoyoteTime = 0.f;
	}

	// Enter a slide when crouch is pressed while running fast on walkable ground. Walking-only (NOT
	// Falling) is deliberate: re-entering from Falling would instantly cancel a slide-hop, because DoJump
	// drops us to MOVE_Falling and this check runs right afterwards in the same move.
	if (MovementMode == MOVE_Walking && bWantsToCrouch && CanSlide())
	{
		SetMovementMode(MOVE_Custom, CMOVE_Slide);
	}
}

void ULemonCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (IsSliding())
	{
		EnterSlide();
	}
	else if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Slide)
	{
		ExitSlide();
	}
}

void ULemonCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);

	if (CustomMovementMode == CMOVE_Slide)
	{
		PhysSlide(DeltaTime, Iterations);
	}
}

void ULemonCharacterMovementComponent::PhysSlide(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const FLemonSlideSettings& Slide = GetSlideSettings();

	// Sub-step loop, mirroring PhysWalking so behaviour is framerate-independent.
	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner
		&& (CharacterOwner->Controller || bRunPhysicsWithNoController || CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy))
	{
		Iterations++;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Exit to walking if crouch is released or we've slowed below the threshold (3D speed so a
		// fast descent down a slope keeps the slide alive).
		if (!bWantsToCrouch || Velocity.SizeSquared() < FMath::Square(Slide.MinSpeed))
		{
			SetMovementMode(MOVE_Walking);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();

		// A slide needs a walkable surface beneath us.
		FindFloor(OldLocation, CurrentFloor, false);
		if (!CurrentFloor.IsWalkableFloor())
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}
		const FVector FloorNormal = CurrentFloor.HitResult.ImpactNormal;

		RestorePreAdditiveRootMotionVelocity();

		// Bleed speed with low slide friction + braking ONLY. Movement input must not add thrust, so
		// CalcVelocity runs with zero acceleration (a slide is carried momentum, not driven movement).
		const FVector RealAcceleration = Acceleration;
		Acceleration = FVector::ZeroVector;
		CalcVelocity(timeTick, Slide.Friction, false, GetMaxBrakingDeceleration());
		Acceleration = RealAcceleration;

		// Steering: gently rotate the slide's heading toward sideways input WITHOUT changing speed.
		// Forward/back input contributes nothing (SteerSign ~ 0); only the sideways component turns you.
		if (!RealAcceleration.IsNearlyZero() && !Velocity.IsNearlyZero())
		{
			const FVector VelDir = Velocity.GetSafeNormal2D();
			const FVector InputDir = RealAcceleration.GetSafeNormal2D();
			const float SteerSign = FVector::CrossProduct(VelDir, InputDir).Z; // -1..1, 0 when straight ahead
			Velocity = Velocity.RotateAngleAxis(SteerSign * Slide.SteerRate * timeTick, FVector::UpVector);
		}

		// Slope acceleration: gravity along the floor plane (downhill speeds up, uphill slows). Keep the
		// velocity horizontal afterwards — MoveAlongFloor does its OWN ramp adjustment, so feeding it
		// slope-projected velocity double-counts the slope, drives the capsule into the ramp and stalls
		// the slide (the flicker bug).
		const FVector SlopeAccel = FVector::VectorPlaneProject(GetGravityDirection(), FloorNormal) * Slide.SlopeForce;
		Velocity += SlopeAccel * timeTick;
		MaintainHorizontalGroundVelocity();

		ApplyRootMotionToVelocity(timeTick);

		// Move along the floor (handles ramps, steps and surface sliding).
		FStepDownResult StepDownResult;
		MoveAlongFloor(Velocity, timeTick, &StepDownResult);

		if (MovementMode != MOVE_Custom)
		{
			// MoveAlongFloor changed the mode (e.g. entered water); let the new mode take over.
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		// Refresh the floor after the move.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		}

		if (!CurrentFloor.IsWalkableFloor())
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);

		// Derive velocity from the actual displacement so collisions / ramps bleed speed correctly.
		if (!bJustTeleported && timeTick >= MIN_TICK_TIME)
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
		}
		MaintainHorizontalGroundVelocity();
	}
}
