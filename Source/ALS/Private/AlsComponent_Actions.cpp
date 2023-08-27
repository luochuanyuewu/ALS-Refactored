#include "AlsComponent.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/NetConnection.h"
#include "Net/Core/PushModel/PushModel.h"
#include "RootMotionSources/AlsRootMotionSource_Mantling.h"
#include "Settings/AlsCharacterSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsUtility.h"

void UAlsComponent::TryStartRolling(const float PlayRate)
{
	if (LocomotionMode == AlsLocomotionModeTags::Grounded)
	{
		StartRolling(PlayRate, Settings->Rolling.bRotateToInputOnStart && LocomotionState.bHasInput
			                       ? LocomotionState.InputYawAngle
			                       : UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw)));
	}
}

bool UAlsComponent::IsRollingAllowedToStart(const UAnimMontage* Montage) const
{
	return !LocomotionAction.IsValid() ||
	       (LocomotionAction == AlsLocomotionActionTags::Rolling &&
	        !OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Montage));
}

void UAlsComponent::StartRolling(const float PlayRate, const float TargetYawAngle)
{
	if (OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy)
	{
		return;
	}

	auto* Montage{SelectRollMontage()};

	if (!ALS_ENSURE(IsValid(Montage)) || !IsRollingAllowedToStart(Montage))
	{
		return;
	}

	const auto StartYawAngle{UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw))};

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		MulticastStartRolling(Montage, PlayRate, StartYawAngle, TargetYawAngle);
	}
	else
	{
		OwnerCharacter->GetCharacterMovement()->FlushServerMoves();

		StartRollingImplementation(Montage, PlayRate, StartYawAngle, TargetYawAngle);
		ServerStartRolling(Montage, PlayRate, StartYawAngle, TargetYawAngle);
	}
}

UAnimMontage* UAlsComponent::SelectRollMontage_Implementation()
{
	return Settings->Rolling.Montage;
}

void UAlsComponent::ServerStartRolling_Implementation(UAnimMontage* Montage, const float PlayRate,
                                                      const float StartYawAngle, const float TargetYawAngle)
{
	if (IsRollingAllowedToStart(Montage))
	{
		MulticastStartRolling(Montage, PlayRate, StartYawAngle, TargetYawAngle);
		OwnerCharacter->ForceNetUpdate();
	}
}

void UAlsComponent::MulticastStartRolling_Implementation(UAnimMontage* Montage, const float PlayRate,
                                                         const float StartYawAngle, const float TargetYawAngle)
{
	StartRollingImplementation(Montage, PlayRate, StartYawAngle, TargetYawAngle);
}

void UAlsComponent::StartRollingImplementation(UAnimMontage* Montage, const float PlayRate,
                                               const float StartYawAngle, const float TargetYawAngle)
{
	if (IsRollingAllowedToStart(Montage) && OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Play(Montage, PlayRate))
	{
		RollingState.TargetYawAngle = TargetYawAngle;

		RefreshRotationInstant(StartYawAngle);

		SetLocomotionAction(AlsLocomotionActionTags::Rolling);
	}
}

void UAlsComponent::RefreshRolling(const float DeltaTime)
{
	if (OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy ||
	    OwnerCharacter->GetMesh()->GetAnimInstance()->RootMotionMode <= ERootMotionMode::IgnoreRootMotion)
	{
		// Refresh rolling physics here because UAlsComponent::PhysicsRotation()
		// won't be called on simulated proxies or with ignored root motion.

		RefreshRollingPhysics(DeltaTime);
	}
}

void UAlsComponent::RefreshRollingPhysics(const float DeltaTime)
{
	if (LocomotionAction != AlsLocomotionActionTags::Rolling)
	{
		return;
	}

	auto TargetRotation{OwnerCharacter->GetCharacterMovement()->UpdatedComponent->GetComponentRotation()};

	if (Settings->Rolling.RotationInterpolationSpeed <= 0.0f)
	{
		TargetRotation.Yaw = RollingState.TargetYawAngle;

		OwnerCharacter->GetCharacterMovement()->MoveUpdatedComponent(FVector::ZeroVector, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	else
	{
		TargetRotation.Yaw = UAlsMath::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(TargetRotation.Yaw)),
		                                                     RollingState.TargetYawAngle, DeltaTime,
		                                                     Settings->Rolling.RotationInterpolationSpeed);

		OwnerCharacter->GetCharacterMovement()->MoveUpdatedComponent(FVector::ZeroVector, TargetRotation, false);
	}
}

bool UAlsComponent::TryStartMantlingGrounded()
{
	return LocomotionMode == AlsLocomotionModeTags::Grounded &&
	       TryStartMantling(Settings->Mantling.GroundedTrace);
}

bool UAlsComponent::TryStartMantlingInAir()
{
	return LocomotionMode == AlsLocomotionModeTags::InAir && OwnerCharacter->IsLocallyControlled() &&
	       TryStartMantling(Settings->Mantling.InAirTrace);
}

bool UAlsComponent::IsMantlingAllowedToStart_Implementation() const
{
	return !LocomotionAction.IsValid();
}

bool UAlsComponent::TryStartMantling(const FAlsMantlingTraceSettings& TraceSettings)
{
	if (!Settings->Mantling.bAllowMantling || OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy || !IsMantlingAllowedToStart())
	{
		return false;
	}

	const auto ActorLocation{OwnerCharacter->GetActorLocation()};
	const auto ActorYawAngle{UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw))};

	float ForwardTraceAngle;
	if (LocomotionState.bHasSpeed)
	{
		ForwardTraceAngle = LocomotionState.bHasInput
			                    ? LocomotionState.VelocityYawAngle +
			                      FMath::ClampAngle(LocomotionState.InputYawAngle - LocomotionState.VelocityYawAngle,
			                                        -Settings->Mantling.MaxReachAngle, Settings->Mantling.MaxReachAngle)
			                    : LocomotionState.VelocityYawAngle;
	}
	else
	{
		ForwardTraceAngle = LocomotionState.bHasInput ? LocomotionState.InputYawAngle : ActorYawAngle;
	}

	const auto ForwardTraceDeltaAngle{FRotator3f::NormalizeAxis(ForwardTraceAngle - ActorYawAngle)};
	if (FMath::Abs(ForwardTraceDeltaAngle) > Settings->Mantling.TraceAngleThreshold)
	{
		return false;
	}

	const auto ForwardTraceDirection{
		UAlsMath::AngleToDirectionXY(
			ActorYawAngle + FMath::ClampAngle(ForwardTraceDeltaAngle, -Settings->Mantling.MaxReachAngle, Settings->Mantling.MaxReachAngle))
	};

#if ENABLE_DRAW_DEBUG
	const auto bDisplayDebug{UAlsUtility::ShouldDisplayDebugForActor(OwnerCharacter, UAlsConstants::MantlingDebugDisplayName())};
#endif

	const auto* Capsule{OwnerCharacter->GetCapsuleComponent()};

	const auto CapsuleScale{Capsule->GetComponentScale().Z};
	const auto CapsuleRadius{Capsule->GetScaledCapsuleRadius()};
	const auto CapsuleHalfHeight{Capsule->GetScaledCapsuleHalfHeight()};

	const FVector CapsuleBottomLocation{ActorLocation.X, ActorLocation.Y, ActorLocation.Z - CapsuleHalfHeight};

	const auto TraceCapsuleRadius{CapsuleRadius - 1.0f};

	const auto LedgeHeightDelta{UE_REAL_TO_FLOAT((TraceSettings.LedgeHeight.GetMax() - TraceSettings.LedgeHeight.GetMin()) * CapsuleScale)};

	// Trace forward to find an object the character cannot walk on.

	static const FName ForwardTraceTag{FString::Printf(TEXT("%hs (Forward Trace)"), __FUNCTION__)};

	auto ForwardTraceStart{CapsuleBottomLocation - ForwardTraceDirection * CapsuleRadius};
	ForwardTraceStart.Z += (TraceSettings.LedgeHeight.X + TraceSettings.LedgeHeight.Y) *
		0.5f * CapsuleScale - UCharacterMovementComponent::MAX_FLOOR_DIST;

	auto ForwardTraceEnd{ForwardTraceStart + ForwardTraceDirection * (CapsuleRadius + (TraceSettings.ReachDistance + 1.0f) * CapsuleScale)};

	const auto ForwardTraceCapsuleHalfHeight{LedgeHeightDelta * 0.5f};

	FHitResult ForwardTraceHit;
	GetWorld()->SweepSingleByChannel(ForwardTraceHit, ForwardTraceStart, ForwardTraceEnd, FQuat::Identity, ECC_WorldStatic,
									 FCollisionShape::MakeCapsule(TraceCapsuleRadius, ForwardTraceCapsuleHalfHeight),
									 {ForwardTraceTag, false, OwnerCharacter}, Settings->Mantling.MantlingTraceResponses);

	auto* TargetPrimitive{ForwardTraceHit.GetComponent()};

	if (!ForwardTraceHit.IsValidBlockingHit() ||
	    !IsValid(TargetPrimitive) ||
	    TargetPrimitive->GetComponentVelocity().SizeSquared() > FMath::Square(Settings->Mantling.TargetPrimitiveSpeedThreshold) ||
	    !TargetPrimitive->CanCharacterStepUp(OwnerCharacter) ||
	    OwnerCharacter->GetCharacterMovement()->IsWalkable(ForwardTraceHit))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsUtility::DrawDebugSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, false, ForwardTraceHit, {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);
		}
#endif

		return false;
	}

	// Trace downward from the first trace's impact point and determine if the hit location is walkable.

	static const FName DownwardTraceTag{FString::Printf(TEXT("%hs (Downward Trace)"), __FUNCTION__)};

	const auto TargetLocationOffset{
		FVector2D{ForwardTraceHit.ImpactNormal.GetSafeNormal2D()} * (TraceSettings.TargetLocationOffset * CapsuleScale)
	};

	const FVector DownwardTraceStart{
		ForwardTraceHit.ImpactPoint.X - TargetLocationOffset.X,
		ForwardTraceHit.ImpactPoint.Y - TargetLocationOffset.Y,
		CapsuleBottomLocation.Z + LedgeHeightDelta + 2.5f * TraceCapsuleRadius + UCharacterMovementComponent::MIN_FLOOR_DIST
	};

	const FVector DownwardTraceEnd{
		DownwardTraceStart.X,
		DownwardTraceStart.Y,
		CapsuleBottomLocation.Z +
		TraceSettings.LedgeHeight.GetMin() * CapsuleScale + TraceCapsuleRadius - UCharacterMovementComponent::MAX_FLOOR_DIST
	};

	FHitResult DownwardTraceHit;
	GetWorld()->SweepSingleByChannel(DownwardTraceHit, DownwardTraceStart, DownwardTraceEnd, FQuat::Identity,
									 ECC_WorldStatic, FCollisionShape::MakeSphere(TraceCapsuleRadius),
									 {DownwardTraceTag, false, OwnerCharacter}, Settings->Mantling.MantlingTraceResponses);

	if (!OwnerCharacter->GetCharacterMovement()->IsWalkable(DownwardTraceHit))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsUtility::DrawDebugSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit, {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);

			UAlsUtility::DrawDebugSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd, TraceCapsuleRadius,
			                                        false, DownwardTraceHit, {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f},
			                                        TraceSettings.bDrawFailedTraces ? 7.5f : 0.0f);
		}
#endif

		return false;
	}

	// Check if the capsule has room to stand at the downward trace's location. If so,
	// set that location as the target transform and calculate the mantling height.

	static const FName FreeSpaceTraceTag{FString::Printf(TEXT("%hs (Free Space Overlap)"), __FUNCTION__)};

	const FVector TargetLocation{
		DownwardTraceHit.Location.X,
		DownwardTraceHit.Location.Y,
		DownwardTraceHit.ImpactPoint.Z + UCharacterMovementComponent::MIN_FLOOR_DIST
	};

	const FVector TargetCapsuleLocation{TargetLocation.X, TargetLocation.Y, TargetLocation.Z + CapsuleHalfHeight};

	if (GetWorld()->OverlapBlockingTestByChannel(TargetCapsuleLocation, FQuat::Identity, ECC_WorldStatic,
												 FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
												 {FreeSpaceTraceTag, false, OwnerCharacter}, Settings->Mantling.MantlingTraceResponses))
	{
#if ENABLE_DRAW_DEBUG
		if (bDisplayDebug)
		{
			UAlsUtility::DrawDebugSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
			                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit, {0.0f, 0.25f, 1.0f},
			                                                    {0.0f, 0.75f, 1.0f}, TraceSettings.bDrawFailedTraces ? 5.0f : 0.0f);

			UAlsUtility::DrawDebugSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd, TraceCapsuleRadius,
			                                        false, DownwardTraceHit, {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f},
			                                        TraceSettings.bDrawFailedTraces ? 7.5f : 0.0f);

			DrawDebugCapsule(GetWorld(), TargetCapsuleLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity,
			                 FColor::Red, false, TraceSettings.bDrawFailedTraces ? 10.0f : 0.0f);
		}
#endif

		return false;
	}

#if ENABLE_DRAW_DEBUG
	if (bDisplayDebug)
	{
		UAlsUtility::DrawDebugSweepSingleCapsuleAlternative(GetWorld(), ForwardTraceStart, ForwardTraceEnd, TraceCapsuleRadius,
		                                                    ForwardTraceCapsuleHalfHeight, true, ForwardTraceHit,
		                                                    {0.0f, 0.25f, 1.0f}, {0.0f, 0.75f, 1.0f}, 5.0f);

		UAlsUtility::DrawDebugSweepSingleSphere(GetWorld(), DownwardTraceStart, DownwardTraceEnd,
		                                        TraceCapsuleRadius, true, DownwardTraceHit,
		                                        {0.25f, 0.0f, 1.0f}, {0.75f, 0.0f, 1.0f}, 7.5f);
	}
#endif

	const auto TargetRotation{(-ForwardTraceHit.ImpactNormal.GetSafeNormal2D()).ToOrientationQuat()};

	FAlsMantlingParameters Parameters;

	Parameters.TargetPrimitive = TargetPrimitive;
	Parameters.MantlingHeight = UE_REAL_TO_FLOAT((TargetLocation.Z - CapsuleBottomLocation.Z) / CapsuleScale);

	// Determine the mantling type by checking the movement mode and mantling height.

	Parameters.MantlingType = LocomotionMode != AlsLocomotionModeTags::Grounded
		                          ? EAlsMantlingType::InAir
		                          : Parameters.MantlingHeight > Settings->Mantling.MantlingHighHeightThreshold
		                          ? EAlsMantlingType::High
		                          : EAlsMantlingType::Low;

	// If the target primitive can't move, then use world coordinates to save
	// some performance by skipping some coordinate space transformations later.

	if (MovementBaseUtility::UseRelativeLocation(TargetPrimitive))
	{
		const auto TargetRelativeTransform{
			TargetPrimitive->GetComponentTransform().GetRelativeTransform({TargetRotation, TargetLocation})
		};

		Parameters.TargetRelativeLocation = TargetRelativeTransform.GetLocation();
		Parameters.TargetRelativeRotation = TargetRelativeTransform.Rotator();
	}
	else
	{
		Parameters.TargetRelativeLocation = TargetLocation;
		Parameters.TargetRelativeRotation = TargetRotation.Rotator();
	}

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		MulticastStartMantling(Parameters);
	}
	else
	{
		OwnerCharacter->GetCharacterMovement()->FlushServerMoves();

		StartMantlingImplementation(Parameters);
		ServerStartMantling(Parameters);
	}

	return true;
}

void UAlsComponent::ServerStartMantling_Implementation(const FAlsMantlingParameters& Parameters)
{
	if (IsMantlingAllowedToStart())
	{
		MulticastStartMantling(Parameters);
		OwnerCharacter->ForceNetUpdate();
	}
}

void UAlsComponent::MulticastStartMantling_Implementation(const FAlsMantlingParameters& Parameters)
{
	StartMantlingImplementation(Parameters);
}

void UAlsComponent::StartMantlingImplementation(const FAlsMantlingParameters& Parameters)
{
	if (!IsMantlingAllowedToStart())
	{
		return;
	}

	auto* MantlingSettings{SelectMantlingSettings(Parameters.MantlingType)};

	if (!ALS_ENSURE(IsValid(MantlingSettings)) ||
	    !ALS_ENSURE(IsValid(MantlingSettings->BlendInCurve)) ||
	    !ALS_ENSURE(IsValid(MantlingSettings->InterpolationAndCorrectionAmountsCurve)))
	{
		return;
	}

	const auto StartTime{MantlingSettings->GetStartTimeForHeight(Parameters.MantlingHeight)};
	const auto PlayRate{MantlingSettings->GetPlayRateForHeight(Parameters.MantlingHeight)};

	// Calculate mantling duration.

	auto MinTime{0.0f};
	auto MaxTime{0.0f};
	MantlingSettings->InterpolationAndCorrectionAmountsCurve->GetTimeRange(MinTime, MaxTime);

	const auto Duration{MaxTime - StartTime};

	// Calculate actor offsets (offsets between actor and target transform).

	const auto bUseRelativeLocation{MovementBaseUtility::UseRelativeLocation(Parameters.TargetPrimitive.Get())};
	const auto TargetRelativeRotation{Parameters.TargetRelativeRotation.GetNormalized()};

	const auto TargetTransform{
		bUseRelativeLocation
			? FTransform{
				TargetRelativeRotation, Parameters.TargetRelativeLocation,
				Parameters.TargetPrimitive->GetComponentScale()
			}.GetRelativeTransformReverse(Parameters.TargetPrimitive->GetComponentTransform())
			: FTransform{TargetRelativeRotation, Parameters.TargetRelativeLocation}
	};

	const auto ActorFeetLocationOffset{OwnerCharacter->GetCharacterMovement()->GetActorFeetLocation() - TargetTransform.GetLocation()};
	const auto ActorRotationOffset{TargetTransform.GetRotation().Inverse() * OwnerCharacter->GetActorQuat()};

	// Clear the character movement mode and set the locomotion action to mantling.

	OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Custom);
	OwnerCharacter->GetCharacterMovement()->SetBase(Parameters.TargetPrimitive.Get());
	AlsCharacterMovement->SetMovementModeLocked(true);

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		OwnerCharacter->GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;

		OwnerCharacter->GetMesh()->SetRelativeLocationAndRotation(OwnerCharacter->GetBaseTranslationOffset(),
												  OwnerCharacter->GetMesh()->IsUsingAbsoluteRotation()
													  ? OwnerCharacter->GetActorTransform().GetRotation() * OwnerCharacter->GetBaseRotationOffset()
													  : OwnerCharacter->GetBaseRotationOffset());
	}

	// Apply mantling root motion.

	const auto Mantling{MakeShared<FAlsRootMotionSource_Mantling>()};
	Mantling->InstanceName = __FUNCTION__;
	Mantling->Duration = Duration / PlayRate;
	Mantling->MantlingSettings = MantlingSettings;
	Mantling->TargetPrimitive = bUseRelativeLocation ? Parameters.TargetPrimitive : nullptr;
	Mantling->TargetRelativeLocation = Parameters.TargetRelativeLocation;
	Mantling->TargetRelativeRotation = TargetRelativeRotation;
	Mantling->ActorFeetLocationOffset = ActorFeetLocationOffset;
	Mantling->ActorRotationOffset = ActorRotationOffset.Rotator();
	Mantling->MantlingHeight = Parameters.MantlingHeight;

	MantlingRootMotionSourceId = OwnerCharacter->GetCharacterMovement()->ApplyRootMotionSource(Mantling);

	// Play the animation montage if valid.

	if (ALS_ENSURE(IsValid(MantlingSettings->Montage)))
	{
		// TODO Magic. I can't explain why, but this code fixes animation and root motion source desynchronization.

		const auto MontageStartTime{
			Parameters.MantlingType == EAlsMantlingType::InAir && OwnerCharacter->IsLocallyControlled()
				? StartTime - FMath::GetMappedRangeValueClamped(
					  FVector2f{MantlingSettings->ReferenceHeight}, {GetWorld()->GetDeltaSeconds(), 0.0f}, Parameters.MantlingHeight)
				: StartTime
		};

		if (OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Play(MantlingSettings->Montage, PlayRate,
		                                               EMontagePlayReturnType::MontageLength,
		                                               MontageStartTime, false))
		{
			SetLocomotionAction(AlsLocomotionActionTags::Mantling);
		}
	}

	OnMantlingStarted(Parameters);
}

UAlsMantlingSettings* UAlsComponent::SelectMantlingSettings_Implementation(EAlsMantlingType MantlingType)
{
	return nullptr;
}

void UAlsComponent::OnMantlingStarted_Implementation(const FAlsMantlingParameters& Parameters)
{
	OnMantlingStartedEvent.Broadcast(Parameters);
}

void UAlsComponent::RefreshMantling()
{
	if (MantlingRootMotionSourceId <= 0)
	{
		return;
	}

	const auto RootMotionSource{OwnerCharacter->GetCharacterMovement()->GetRootMotionSourceByID(MantlingRootMotionSourceId)};

	if (!RootMotionSource.IsValid() ||
	    RootMotionSource->Status.HasFlag(ERootMotionSourceStatusFlags::Finished) ||
	    RootMotionSource->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval) ||
	    (LocomotionAction.IsValid() && LocomotionAction != AlsLocomotionActionTags::Mantling) ||
	    OwnerCharacter->GetCharacterMovement()->MovementMode != MOVE_Custom)
	{
		StopMantling();
		OwnerCharacter->ForceNetUpdate();
	}
}

void UAlsComponent::StopMantling()
{
	if (MantlingRootMotionSourceId <= 0)
	{
		return;
	}

	const auto RootMotionSource{OwnerCharacter->GetCharacterMovement()->GetRootMotionSourceByID(MantlingRootMotionSourceId)};

	if (RootMotionSource.IsValid() &&
	    !RootMotionSource->Status.HasFlag(ERootMotionSourceStatusFlags::Finished) &&
	    !RootMotionSource->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval))
	{
		RootMotionSource->Status.SetFlag(ERootMotionSourceStatusFlags::MarkedForRemoval);
	}

	MantlingRootMotionSourceId = 0;

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		OwnerCharacter->GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
	}

	AlsCharacterMovement->SetMovementModeLocked(false);
	OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	OnMantlingEnded();
}

void UAlsComponent::OnMantlingEnded_Implementation()
{
	OnMantlingEndedEvent.Broadcast();
}

bool UAlsComponent::IsRagdollingAllowedToStart() const
{
	return LocomotionAction != AlsLocomotionActionTags::Ragdolling;
}

void UAlsComponent::StartRagdolling()
{
	if (OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy || !IsRagdollingAllowedToStart())
	{
		return;
	}

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		MulticastStartRagdolling();
	}
	else
	{
		OwnerCharacter->GetCharacterMovement()->FlushServerMoves();

		ServerStartRagdolling();
	}
}

void UAlsComponent::ServerStartRagdolling_Implementation()
{
	if (IsRagdollingAllowedToStart())
	{
		MulticastStartRagdolling();
		OwnerCharacter->ForceNetUpdate();
	}
}

void UAlsComponent::MulticastStartRagdolling_Implementation()
{
	StartRagdollingImplementation();
}

void UAlsComponent::StartRagdollingImplementation()
{
	if (!IsRagdollingAllowedToStart())
	{
		return;
	}

	OwnerCharacter->GetMesh()->bUpdateJointsFromAnimation = true; // Required for the flail animation to work properly.

	if (!OwnerCharacter->GetMesh()->IsRunningParallelEvaluation() && OwnerCharacter->GetMesh()->GetBoneSpaceTransforms().Num() > 0)
	{
		OwnerCharacter->GetMesh()->UpdateRBJointMotors();
	}

	if (!IsNetMode(NM_Client))
	{
		// This is necessary to keep the animation instance ticking on the server during ragdolling.

		OwnerCharacter->GetMesh()->bOnlyAllowAutonomousTickPose = false;
	}

	// Stop any active montages.

	static constexpr auto BlendOutTime{0.2f};

	OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Stop(BlendOutTime);

	// When networked, disable replicate movement, reset ragdolling target location, and pull force variables.

	OwnerCharacter->SetReplicateMovement(false);
	OwnerCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true;

	// Clear the character movement mode and set the locomotion action to ragdolling.

	OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_None);
	AlsCharacterMovement->SetMovementModeLocked(true);

	SetLocomotionAction(AlsLocomotionActionTags::Ragdolling);

	// Disable capsule collision and enable mesh physics simulation starting from the pelvis.

	OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OwnerCharacter->GetMesh()->SetCollisionObjectType(ECC_PhysicsBody);
	OwnerCharacter->GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	OwnerCharacter->GetMesh()->SetAllBodiesBelowSimulatePhysics(UAlsConstants::PelvisBoneName(), true, true);

	if (Settings->Ragdolling.bLimitInitialRagdollSpeed)
	{
		// Limit the ragdoll's speed for a few frames, because for some unclear reason,
		// it can get a much higher initial speed than the character's last speed.

		// TODO Find a better solution or wait for a fix in future engine versions.

		static constexpr auto MinSpeedLimit{200.0f};

		RagdollingState.SpeedLimitFrameTimeRemaining = 8;
		RagdollingState.SpeedLimit = FMath::Max(LocomotionState.Velocity.Size(), MinSpeedLimit);

		OwnerCharacter->GetMesh()->ForEachBodyBelow(UAlsConstants::PelvisBoneName(), true, false,
									[SpeedLimit = RagdollingState.SpeedLimit](FBodyInstance* Body)
									{
										Body->SetLinearVelocity(Body->GetUnrealWorldVelocity().GetClampedToMaxSize(SpeedLimit), false);
									});
	}

	RagdollingState.PullForce = 0.0f;
	RagdollingState.bPendingFinalization = false;

	if (OwnerCharacter->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		SetRagdollTargetLocation(OwnerCharacter->GetMesh()->GetSocketLocation(UAlsConstants::PelvisBoneName()));
	}

	OnRagdollingStarted();
}

void UAlsComponent::OnRagdollingStarted_Implementation()
{
	OnRagdollingStartedEvent.Broadcast();
}

void UAlsComponent::SetRagdollTargetLocation(const FVector& NewTargetLocation)
{
	if (RagdollTargetLocation != NewTargetLocation)
	{
		RagdollTargetLocation = NewTargetLocation;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, RagdollTargetLocation, this)

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetRagdollTargetLocation(RagdollTargetLocation);
		}
	}
}

void UAlsComponent::ServerSetRagdollTargetLocation_Implementation(const FVector_NetQuantize100& NewTargetLocation)
{
	SetRagdollTargetLocation(NewTargetLocation);
}

void UAlsComponent::RefreshRagdolling(const float DeltaTime)
{
	if (LocomotionAction != AlsLocomotionActionTags::Ragdolling)
	{
		return;
	}

	if (RagdollingState.SpeedLimitFrameTimeRemaining > 0)
	{
		OwnerCharacter->GetMesh()->ForEachBodyBelow(UAlsConstants::PelvisBoneName(), true, false,
		                            [SpeedLimit = RagdollingState.SpeedLimit](FBodyInstance* Body)
		                            {
			                            Body->SetLinearVelocity(Body->GetUnrealWorldVelocity().GetClampedToMaxSize(SpeedLimit), false);
		                            });

		RagdollingState.SpeedLimitFrameTimeRemaining -= 1;
	}

	if (IsNetMode(NM_DedicatedServer))
	{
		// Change animation tick option when the host is a dedicated server to avoid z-location issue.

		OwnerCharacter->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	RagdollingState.RootBoneVelocity = OwnerCharacter->GetMesh()->GetPhysicsLinearVelocity(UAlsConstants::RootBoneName());

	// Use the velocity to scale ragdoll joint strength for physical animation.

	static constexpr auto ReferenceSpeed{1000.0f};
	static constexpr auto Stiffness{25000.0f};

	OwnerCharacter->GetMesh()->SetAllMotorsAngularDriveParams(UAlsMath::Clamp01(UE_REAL_TO_FLOAT(
		                                          RagdollingState.RootBoneVelocity.Size()) / ReferenceSpeed) * Stiffness,
	                                          0.0f, 0.0f, false);

	RefreshRagdollingActorTransform(DeltaTime);
}

void UAlsComponent::RefreshRagdollingActorTransform(const float DeltaTime)
{
	const auto bLocallyControlled{OwnerCharacter->IsLocallyControlled()};
	const auto PelvisTransform{OwnerCharacter->GetMesh()->GetSocketTransform(UAlsConstants::PelvisBoneName())};

	if (bLocallyControlled)
	{
		SetRagdollTargetLocation(PelvisTransform.GetLocation());
	}

	// Trace downward from the target location to offset the target location, preventing the lower
	// half of the capsule from going through the floor when the ragdoll is laying on the ground.

	FHitResult Hit;
	GetWorld()->LineTraceSingleByChannel(Hit, RagdollTargetLocation, {
		                                     RagdollTargetLocation.X,
		                                     RagdollTargetLocation.Y,
		                                     RagdollTargetLocation.Z - OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
	                                     }, ECC_WorldStatic, {__FUNCTION__, false, OwnerCharacter}, Settings->Ragdolling.GroundTraceResponses);

	auto NewActorLocation{RagdollTargetLocation};

	RagdollingState.bGrounded = Hit.IsValidBlockingHit();

	if (RagdollingState.bGrounded)
	{
		NewActorLocation.Z += OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - FMath::Abs(Hit.ImpactPoint.Z - Hit.TraceStart.Z) + 1.0f;
	}

	if (!bLocallyControlled)
	{
		static constexpr auto PullForce{750.0f};
		static constexpr auto InterpolationSpeed{0.6f};

		RagdollingState.PullForce = FMath::FInterpTo(RagdollingState.PullForce, PullForce, DeltaTime, InterpolationSpeed);

		const auto RootBoneHorizontalSpeedSquared{RagdollingState.RootBoneVelocity.SizeSquared2D()};

		const auto PullForceSocketName{
			RootBoneHorizontalSpeedSquared > FMath::Square(300.0f) ? UAlsConstants::Spine03BoneName() : UAlsConstants::PelvisBoneName()
		};

		OwnerCharacter->GetMesh()->AddForce((RagdollTargetLocation - OwnerCharacter->GetMesh()->GetSocketLocation(PullForceSocketName)) * RagdollingState.PullForce,
		                    PullForceSocketName, true);
	}

	// Determine whether the ragdoll is facing upward or downward and set the target rotation accordingly.

	const auto PelvisRotation{PelvisTransform.Rotator()};

	RagdollingState.bFacedUpward = PelvisRotation.Roll <= 0.0f;

	auto NewActorRotation{OwnerCharacter->GetActorRotation()};
	NewActorRotation.Yaw = RagdollingState.bFacedUpward ? PelvisRotation.Yaw - 180.0f : PelvisRotation.Yaw;

	OwnerCharacter->SetActorLocationAndRotation(NewActorLocation, NewActorRotation);
}

bool UAlsComponent::IsRagdollingAllowedToStop() const
{
	return LocomotionAction == AlsLocomotionActionTags::Ragdolling;
}

bool UAlsComponent::TryStopRagdolling()
{
	if (OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy || !IsRagdollingAllowedToStop())
	{
		return false;
	}

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		MulticastStopRagdolling();
	}
	else
	{
		ServerStopRagdolling();
	}

	return true;
}

void UAlsComponent::ServerStopRagdolling_Implementation()
{
	if (IsRagdollingAllowedToStop())
	{
		MulticastStopRagdolling();
		OwnerCharacter->ForceNetUpdate();
	}
}

void UAlsComponent::MulticastStopRagdolling_Implementation()
{
	StopRagdollingImplementation();
}

void UAlsComponent::StopRagdollingImplementation()
{
	if (!IsRagdollingAllowedToStop())
	{
		return;
	}

	AnimationInstance->StopRagdolling();

	RagdollingState.bPendingFinalization = true;

	// If the ragdoll is on the ground, set the movement mode to walking and play a get-up montage. If not, set
	// the movement mode to falling and update the character movement velocity to match the last ragdoll velocity.

	AlsCharacterMovement->SetMovementModeLocked(false);

	if (RagdollingState.bGrounded)
	{
		OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	else
	{
		OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		OwnerCharacter->GetCharacterMovement()->Velocity = RagdollingState.RootBoneVelocity;
	}

	// Re-enable replicate movement.

	OwnerCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
	OwnerCharacter->SetReplicateMovement(true);

	if (!IsNetMode(NM_Client))
	{
		// Restore bOnlyAllowAutonomousTickPose in the same way as in ACharacter::PossessedBy().

		OwnerCharacter->GetMesh()->bOnlyAllowAutonomousTickPose = OwnerCharacter->GetRemoteRole() == ROLE_AutonomousProxy &&
		                                          IsValid(OwnerCharacter->GetNetConnection()) && OwnerCharacter->IsPawnControlled();
	}

	OwnerCharacter->GetMesh()->bUpdateJointsFromAnimation = false;

	SetLocomotionAction(FGameplayTag::EmptyTag);

	OnRagdollingEnded();

	if (RagdollingState.bGrounded &&
	    OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Play(SelectGetUpMontage(RagdollingState.bFacedUpward), 1.0f,
	                                               EMontagePlayReturnType::MontageLength, 0.0f, true))
	{
		SetLocomotionAction(AlsLocomotionActionTags::GettingUp);
	}
}

void UAlsComponent::FinalizeRagdolling()
{
	if (!RagdollingState.bPendingFinalization)
	{
		return;
	}

	RagdollingState.bPendingFinalization = false;

	// Disable physics simulation of a mesh and enable capsule collision.

	OwnerCharacter->GetMesh()->SetAllBodiesSimulatePhysics(false);
	OwnerCharacter->GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OwnerCharacter->GetMesh()->SetCollisionObjectType(ECC_Pawn);

	OwnerCharacter->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

UAnimMontage* UAlsComponent::SelectGetUpMontage_Implementation(const bool bRagdollFacedUpward)
{
	return bRagdollFacedUpward ? Settings->Ragdolling.GetUpBackMontage : Settings->Ragdolling.GetUpFrontMontage;
}

void UAlsComponent::OnRagdollingEnded_Implementation()
{
	OnRagdollingEndedEvent.Broadcast();
}
