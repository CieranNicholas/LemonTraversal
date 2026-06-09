// Copyright Cieran Nicholas. All Rights Reserved.

#include "Movement/LemonCharacterMovementComponent.h"
#include "Movement/LemonMovementSet.h"
#include "Movement/LemonSavedMove.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
	Safe_bWantsToWallRun = 0;
	Safe_CoyoteTime = 0.f;
	Safe_WallRunTime = 0.f;
	Safe_WallRunCooldown = 0.f;
	WallRunNormal = FVector::ZeroVector;
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

void ULemonCharacterMovementComponent::SetWallRunHeld(bool bHeld)
{
	Safe_bWantsToWallRun = bHeld ? 1 : 0;
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
	if (IsWallRunning())
	{
		return GetWallRunSettings().MaxSpeed;
	}
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
	Safe_bWantsToWallRun = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
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
			: IsWallRunning() ? TEXT("WALLRUN")
			: (IsMovingOnGround() ? TEXT("Ground") : (IsFalling() ? TEXT("Falling") : TEXT("Other")));
		GEngine->AddOnScreenDebugMessage(0x1E33, 0.f, FColor::Cyan,
			FString::Printf(TEXT("Lemon | %s | Spd %.0f (2D %.0f) | Floor %d | WantsCrouch %d | Coyote %.2f/%.2f%s | WR t%.2f cd%.2f hold%d"),
				ModeStr, Velocity.Size(), Velocity.Size2D(), CurrentFloor.IsWalkableFloor() ? 1 : 0, (int32)bWantsToCrouch,
				Safe_CoyoteTime, GetCoyoteTime(), CanCoyoteJump() ? TEXT(" [JUMP OK]") : TEXT(""),
				Safe_WallRunTime, Safe_WallRunCooldown, (int32)Safe_bWantsToWallRun));
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
	// Allow jumping out of a slide (slide-hop) or a wall run (wall-jump). The base blocks both because it
	// requires IsMovingOnGround()/IsFalling() (custom modes are neither) and, for slide, bWantsToCrouch is set.
	if (IsSliding() || IsWallRunning())
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

	// Wall-run re-attach cooldown ticks down every move (it's set on a wall-jump or a duration timeout).
	if (Safe_WallRunCooldown > 0.f)
	{
		Safe_WallRunCooldown = FMath::Max(0.f, Safe_WallRunCooldown - DeltaSeconds);
	}

	// Enter a wall run from the air: a near-vertical wall we're holding into, moving fast enough, and past
	// the cooldown. Like the slide-hop, the cooldown also stops a wall-jump from instantly re-attaching.
	if (MovementMode == MOVE_Falling && Safe_WallRunCooldown <= 0.f)
	{
		FVector WallNormal;
		FHitResult WallHit;
		if (CanWallRun(WallNormal, WallHit))
		{
			WallRunNormal = WallNormal; // cache before the mode change so EnterWallRun() can use it
			SetMovementMode(MOVE_Custom, CMOVE_WallRun);
		}
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

	if (IsWallRunning())
	{
		EnterWallRun();
	}
	else if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_WallRun)
	{
		ExitWallRun();
	}
}

void ULemonCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);

	if (CustomMovementMode == CMOVE_Slide)
	{
		PhysSlide(DeltaTime, Iterations);
	}
	else if (CustomMovementMode == CMOVE_WallRun)
	{
		PhysWallRun(DeltaTime, Iterations);
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

/*
	* Wall run (CMOVE_WallRun)
	* A custom movement mode entered from the air. Zero-gravity horizontal run along a near-vertical wall
	* you hold into, capped by MaxDuration, exited by a wall-jump (DoJump override) that rides the existing
	* jump flag. Two predicted scalars (Safe_WallRunTime / Safe_WallRunCooldown) thread through the saved
	* move; the wall normal is re-traced each tick, so it is NOT stored predicted state.
*/

bool ULemonCharacterMovementComponent::IsWallRunning() const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == CMOVE_WallRun);
}

float ULemonCharacterMovementComponent::GetWallRunSide() const
{
	if (!IsWallRunning() || WallRunNormal.IsNearlyZero() || !UpdatedComponent)
	{
		return 0.f;
	}
	// The wall sits opposite its outward normal. Wall on our right => normal points left => dot(Right, N) < 0.
	return (FVector::DotProduct(UpdatedComponent->GetRightVector(), WallRunNormal) < 0.f) ? 1.f : -1.f;
}

const FLemonWallRunSettings& ULemonCharacterMovementComponent::GetWallRunSettings() const
{
	if (MovementSet)
	{
		return MovementSet->WallRun;
	}
	return DefaultWallRunSettings;
}

bool ULemonCharacterMovementComponent::DetectWall(FVector& OutNormal, FHitResult& OutHit) const
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const FLemonWallRunSettings& WR = GetWallRunSettings();

	float CapsuleRadius = 34.f;
	float CapsuleHalfHeight = 88.f;
	if (const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent())
	{
		Capsule->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	}

	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	const FVector Right = UpdatedComponent->GetRightVector();
	const float Reach = CapsuleRadius + WR.WallReach;

	FCollisionQueryParams Params(FName(TEXT("LemonWallRun")), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(Params, ResponseParam);
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();
	const FCollisionShape Shape = GetPawnCapsuleCollisionShape(SHRINK_None);

	FHitResult RightHit(1.f);
	FHitResult LeftHit(1.f);
	const bool bRight = GetWorld()->SweepSingleByChannel(RightHit, Location, Location + Right * Reach, Rotation, Channel, Shape, Params, ResponseParam);
	const bool bLeft = GetWorld()->SweepSingleByChannel(LeftHit, Location, Location - Right * Reach, Rotation, Channel, Shape, Params, ResponseParam);

	// Only near-vertical surfaces count as walls.
	const bool bRightWall = bRight && RightHit.bBlockingHit && FMath::Abs(RightHit.ImpactNormal.Z) <= WR.MaxWallZ;
	const bool bLeftWall = bLeft && LeftHit.bBlockingHit && FMath::Abs(LeftHit.ImpactNormal.Z) <= WR.MaxWallZ;

	if (!bRightWall && !bLeftWall)
	{
		return false;
	}

	bool bChooseRight = bRightWall;
	if (bRightWall && bLeftWall)
	{
		// Both sides hit: pick the one our movement input is pushing toward.
		const FVector InputDir = Acceleration.GetSafeNormal2D();
		const float RightScore = FVector::DotProduct(InputDir, (-RightHit.ImpactNormal).GetSafeNormal2D());
		const float LeftScore = FVector::DotProduct(InputDir, (-LeftHit.ImpactNormal).GetSafeNormal2D());
		bChooseRight = RightScore >= LeftScore;
	}

	OutHit = bChooseRight ? RightHit : LeftHit;
	OutNormal = OutHit.ImpactNormal.GetSafeNormal();
	return true;
}

bool ULemonCharacterMovementComponent::CanWallRun(FVector& OutNormal, FHitResult& OutHit) const
{
	const FLemonWallRunSettings& WR = GetWallRunSettings();

	// Must be holding jump (the engage input) and airborne *because we jumped* — JumpCurrentCount stays 0
	// when you merely walk off a ledge, so this blocks wall-running from a plain fall.
	if (!Safe_bWantsToWallRun || !CharacterOwner || CharacterOwner->JumpCurrentCount <= 0)
	{
		return false;
	}

	// Enough horizontal speed to be worth attaching.
	if (Velocity.Size2D() < WR.MinSpeed)
	{
		return false;
	}

	return DetectWall(OutNormal, OutHit);
}

void ULemonCharacterMovementComponent::EnterWallRun()
{
	const FLemonWallRunSettings& WR = GetWallRunSettings();

	Safe_WallRunTime = 0.f;

	// Zero-G entry: drop the fall, keep the entry speed but redirect it cleanly along the wall plane.
	FVector Horizontal = Velocity;
	Horizontal.Z = 0.f;
	const float EntrySpeed = FMath::Min(Horizontal.Size(), WR.MaxSpeed);

	FVector AlongDir = FVector::VectorPlaneProject(Horizontal, WallRunNormal).GetSafeNormal();
	if (AlongDir.IsNearlyZero())
	{
		AlongDir = Horizontal.GetSafeNormal();
	}
	Velocity = AlongDir * EntrySpeed;
}

void ULemonCharacterMovementComponent::ExitWallRun()
{
	// Clear the cached normal so the camera tilt relaxes; hook point for future wall-run anim/camera state.
	WallRunNormal = FVector::ZeroVector;
}

void ULemonCharacterMovementComponent::PhysWallRun(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const FLemonWallRunSettings& WR = GetWallRunSettings();

	// Sub-step loop, mirroring PhysWalking / PhysSlide so behaviour is framerate-independent.
	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner
		&& (CharacterOwner->Controller || bRunPhysicsWithNoController || CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy))
	{
		Iterations++;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Is there still a wall to run on? If not, drop to falling WITHOUT a cooldown so you can chain to
		// the next wall immediately.
		FVector WallNormal;
		FHitResult WallHit;
		if (!DetectWall(WallNormal, WallHit))
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}
		WallRunNormal = WallNormal;

		const FVector IntoWall = (-WallNormal).GetSafeNormal2D();
		const FVector InputDir = Acceleration.GetSafeNormal2D();

		// Detach if the player actively steers AWAY from the wall, or we've slowed below the threshold.
		// (Sustaining is lenient — you only need to lean in to *start*, then momentum carries the run.)
		const bool bPullingAway = !InputDir.IsNearlyZero() && FVector::DotProduct(InputDir, IntoWall) < -WR.MinInputIntoWall;
		if (bPullingAway || Velocity.Size2D() < WR.MinSpeed)
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		// Zero-G duration cap: after MaxDuration, pop off and start the re-attach cooldown so the same wall
		// can't be ridden forever.
		Safe_WallRunTime += timeTick;
		if (WR.MaxDuration > 0.f && Safe_WallRunTime > WR.MaxDuration)
		{
			Safe_WallRunCooldown = WR.ReattachCooldown;
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();

		RestorePreAdditiveRootMotionVelocity();

		// Horizontal along-wall axis, oriented to our current heading so forward input accelerates "forward".
		FVector WallTangent = FVector::CrossProduct(WallNormal, FVector::UpVector).GetSafeNormal();
		if (FVector::DotProduct(WallTangent, Velocity) < 0.f)
		{
			WallTangent = -WallTangent;
		}

		// Bleed with friction, accelerate along the wall by how aligned input is with the run direction.
		float Speed = Velocity.Size2D();
		Speed -= Speed * WR.Friction * timeTick;
		const float Align = InputDir.IsNearlyZero() ? 0.f : FVector::DotProduct(InputDir, WallTangent);
		if (Align > 0.f)
		{
			Speed += WR.Acceleration * Align * timeTick;
		}
		Speed = FMath::Clamp(Speed, 0.f, WR.MaxSpeed);

		// Compose: run along the wall + a gentle push into it to hold contact, with zero vertical motion.
		Velocity = WallTangent * Speed + IntoWall * WR.StickForce;
		Velocity.Z = 0.f;

		ApplyRootMotionToVelocity(timeTick);

		// Move, handling impacts like PhysFlying (HandleImpact + SlideAlongSurface).
		const FVector Adjusted = Velocity * timeTick;
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

		if (Hit.Time < 1.f)
		{
			HandleImpact(Hit, timeTick, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}

		if (MovementMode != MOVE_Custom)
		{
			// An impact changed the mode (e.g. entered water); let the new mode take over.
			StartNewPhysics(remainingTime, Iterations);
			return;
		}

		// Derive velocity from the actual move (collisions bleed speed), keeping it horizontal (zero-G).
		if (!bJustTeleported && timeTick >= MIN_TICK_TIME)
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
			Velocity.Z = 0.f;
		}

		// Landed (wall met the floor)? Hand back to walking.
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		if (CurrentFloor.IsWalkableFloor() && CurrentFloor.GetDistanceToFloor() < 2.4f)
		{
			SetMovementMode(MOVE_Walking);
			StartNewPhysics(remainingTime, Iterations);
			return;
		}
	}
}

bool ULemonCharacterMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	// Wall-jump: leap out from the wall and up, carrying some along-wall momentum. Rides the existing jump
	// flag (CheckJumpInput -> CanJump -> DoJump), so it needs no new predicted state of its own.
	if (IsWallRunning() && CharacterOwner && CharacterOwner->CanJump())
	{
		const FLemonWallRunSettings& WR = GetWallRunSettings();

		// Prefer a freshly traced wall normal; fall back to the cached one.
		FVector Normal = WallRunNormal;
		FHitResult WallHit;
		FVector Detected;
		if (DetectWall(Detected, WallHit))
		{
			Normal = Detected;
		}
		Normal = Normal.GetSafeNormal2D();
		if (Normal.IsNearlyZero())
		{
			Normal = UpdatedComponent ? UpdatedComponent->GetForwardVector().GetSafeNormal2D() : FVector::ForwardVector;
		}

		// Keep a fraction of the along-wall momentum (strip the into/out-of-wall component first).
		FVector Along = Velocity;
		Along.Z = 0.f;
		Along = FVector::VectorPlaneProject(Along, Normal);

		Velocity = Normal * WR.WallJumpOutSpeed
			+ FVector::UpVector * WR.WallJumpUpSpeed
			+ Along * WR.WallJumpMomentumKeep;

		Safe_WallRunCooldown = WR.ReattachCooldown;
		SetMovementMode(MOVE_Falling);
		return true;
	}

	return Super::DoJump(bReplayingMoves, DeltaTime);
}
