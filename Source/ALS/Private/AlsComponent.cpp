﻿#include "AlsComponent.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacterMovementComponent.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Settings/AlsCharacterSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsComponent)

namespace AlsComponentConstants
{
	inline static constexpr auto TeleportDistanceThresholdSquared{FMath::Square(50.0f)};
}

UAlsComponent::UAlsComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
}

void UAlsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Parameters;
	Parameters.bIsPushBased = true;

	Parameters.Condition = COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredStance, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredGait, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bDesiredAiming, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredRotationMode, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ViewMode, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, OverlayMode, Parameters)

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedViewRotation, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InputDirection, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredVelocityYawAngle, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, RagdollTargetLocation, Parameters)
}

void UAlsComponent::OnRegister()
{
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (OwnerCharacter)
	{
		AlsCharacterMovement = Cast<UAlsCharacterMovementComponent>(OwnerCharacter->GetMovementComponent());
		// Set some default values here to ensure that the animation instance and the
		// camera component can read the most up-to-date values during their initialization.

		RotationMode = bDesiredAiming ? AlsRotationModeTags::Aiming : DesiredRotationMode;
		Stance = DesiredStance;
		Gait = DesiredGait;

		SetReplicatedViewRotation(OwnerCharacter->GetControlRotation());

		ViewState.NetworkSmoothing.InitialRotation = ReplicatedViewRotation;
		ViewState.NetworkSmoothing.Rotation = ReplicatedViewRotation;
		ViewState.Rotation = ReplicatedViewRotation;
		ViewState.PreviousYawAngle = UE_REAL_TO_FLOAT(ReplicatedViewRotation.Yaw);

		const auto& ActorTransform{OwnerCharacter->GetActorTransform()};

		LocomotionState.Location = ActorTransform.GetLocation();
		LocomotionState.RotationQuaternion = ActorTransform.GetRotation();
		LocomotionState.Rotation = OwnerCharacter->GetActorRotation();
		LocomotionState.PreviousYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);

		RefreshTargetYawAngleUsingLocomotionRotation();

		LocomotionState.InputYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);
		LocomotionState.VelocityYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);
	}


	Super::OnRegister();
}

UAlsComponent* UAlsComponent::FindAlsComponent(const AActor* Actor)
{
	return Actor != nullptr ? Actor->FindComponentByClass<UAlsComponent>() : nullptr;
}

bool UAlsComponent::K2_FindAlsComponent(const AActor* Actor, UAlsComponent*& Instance)
{
	if (Actor == nullptr)
	{
		return false;
	}
	Instance = FindAlsComponent(Actor);
	return Instance != nullptr;
}

void UAlsComponent::BeginPlay()
{
	//Post InitializeComponent
	{
		// Make sure the mesh and animation blueprint update after the character to guarantee it gets the most recent values.

		OwnerCharacter->GetMesh()->AddTickPrerequisiteActor(GetOwner());

		if (AlsCharacterMovement == nullptr)
		{
			AlsCharacterMovement = Cast<UAlsCharacterMovementComponent>(OwnerCharacter->GetMovementComponent());

			checkf(AlsCharacterMovement, TEXT("Als Component requires AlsCharacterMovementComponent!"))
		}

		if (AlsCharacterMovement != nullptr && !AlsCharacterMovement->OnPhysicsRotation.IsBoundToObject(this))
		{
			AlsCharacterMovement->OnPhysicsRotation.AddUObject(this, &ThisClass::CharacterMovement_OnPhysicsRotation);
		}

		// Pass current movement settings to the movement component.

		AlsCharacterMovement->SetMovementSettings(MovementSettings);

		AnimationInstance = Cast<UAlsAnimationInstance>(OwnerCharacter->GetMesh()->GetAnimInstance());
	}

	ALS_ENSURE(IsValid(Settings));
	ALS_ENSURE(IsValid(MovementSettings));
	ALS_ENSURE(AnimationInstance.IsValid());

	ALS_ENSURE_MESSAGE(!OwnerCharacter->bUseControllerRotationPitch && !OwnerCharacter->bUseControllerRotationYaw && !OwnerCharacter->bUseControllerRotationRoll,
	                   TEXT("These settings are not allowed and must be turned off!"));


	Super::BeginPlay();

	OwnerCharacter->MovementModeChangedDelegate.AddDynamic(this, &ThisClass::OnMovementModeChanged);
	OnMovementModeChanged(OwnerCharacter, OwnerCharacter->GetCharacterMovement()->GetGroundMovementMode(), 0);

	if (OwnerCharacter->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		// Teleportation of simulated proxies is detected differently, see
		// AAlsCharacter::PostNetReceiveLocationAndRotation() and AAlsCharacter::OnRep_ReplicatedBasedMovement().

		OwnerCharacter->GetCapsuleComponent()->TransformUpdated.AddWeakLambda(
			this, [this](USceneComponent*, const EUpdateTransformFlags, const ETeleportType TeleportType)
			{
				if (TeleportType != ETeleportType::None && AnimationInstance.IsValid())
				{
					AnimationInstance->MarkTeleported();
				}
			});
	}

	RefreshUsingAbsoluteRotation();
	RefreshVisibilityBasedAnimTickOption();

	ViewState.NetworkSmoothing.bEnabled |= IsValid(Settings) &&
		Settings->View.bEnableNetworkSmoothing && OwnerCharacter->GetLocalRole() == ROLE_SimulatedProxy;

	// Update states to use the initial desired values.

	RefreshRotationMode();

	AlsCharacterMovement->SetRotationMode(RotationMode);

	ApplyDesiredStance();

	AlsCharacterMovement->SetStance(Stance);

	RefreshGait();

	OnOverlayModeChanged(OverlayMode);
}

void UAlsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwnerCharacter)
	{
		OwnerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::OnMovementModeChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void UAlsComponent::PostNetReceiveLocationAndRotation()
{
	// AActor::PostNetReceiveLocationAndRotation() function is only called on simulated proxies, so there is no need to check roles here.

	const auto PreviousLocation{OwnerCharacter->GetActorLocation()};

	// Ignore server-replicated rotation on simulated proxies because ALS itself has full control over character rotation.

	OwnerCharacter->GetReplicatedMovement_Mutable().Rotation = OwnerCharacter->GetActorRotation();

	// Super::PostNetReceiveLocationAndRotation();

	// Detect teleportation of simulated proxies.

	auto bTeleported{static_cast<bool>(OwnerCharacter->bSimGravityDisabled)};

	if (!bTeleported && !OwnerCharacter->GetReplicatedBasedMovement().HasRelativeLocation())
	{
		const auto NewLocation{FRepMovement::RebaseOntoLocalOrigin(OwnerCharacter->GetReplicatedBasedMovement().Location, this)};

		bTeleported |= FVector::DistSquared(PreviousLocation, NewLocation) > AlsComponentConstants::TeleportDistanceThresholdSquared;
	}

	if (bTeleported && AnimationInstance.IsValid())
	{
		AnimationInstance->MarkTeleported();
	}
}

void UAlsComponent::OnRep_ReplicatedBasedMovement(FBasedMovementInfo& ReplicatedBasedMovement)
{
	// ACharacter::OnRep_ReplicatedBasedMovement() is only called on simulated proxies, so there is no need to check roles here.

	const auto PreviousLocation{OwnerCharacter->GetActorLocation()};

	// Ignore server-replicated rotation on simulated proxies because ALS itself has full control over character rotation.

	if (OwnerCharacter->GetReplicatedBasedMovement().HasRelativeRotation())
	{
		FVector MovementBaseLocation;
		FQuat MovementBaseRotation;

		MovementBaseUtility::GetMovementBaseTransform(OwnerCharacter->GetReplicatedBasedMovement().MovementBase, OwnerCharacter->GetReplicatedBasedMovement().BoneName,
		                                              MovementBaseLocation, MovementBaseRotation);

		ReplicatedBasedMovement.Rotation = (MovementBaseRotation.Inverse() * OwnerCharacter->GetActorQuat()).Rotator();
	}
	else
	{
		ReplicatedBasedMovement.Rotation = OwnerCharacter->GetActorRotation();
	}

	// Super::OnRep_ReplicatedBasedMovement();

	// Detect teleportation of simulated proxies.

	auto bTeleported{static_cast<bool>(OwnerCharacter->bSimGravityDisabled)};

	if (!bTeleported && OwnerCharacter->GetBasedMovement().HasRelativeLocation())
	{
		const auto NewLocation{
			OwnerCharacter->GetCharacterMovement()->OldBaseLocation + OwnerCharacter->GetCharacterMovement()->OldBaseQuat.RotateVector(OwnerCharacter->GetBasedMovement().Location)
		};

		bTeleported |= FVector::DistSquared(PreviousLocation, NewLocation) > AlsComponentConstants::TeleportDistanceThresholdSquared;
	}

	if (bTeleported && AnimationInstance.IsValid())
	{
		AnimationInstance->MarkTeleported();
	}
}

void UAlsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UAlsComponent::Tick()"), STAT_UAlsComponent_Tick, STATGROUP_Als)

	if (!IsValid(Settings) || !AnimationInstance.IsValid())
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		return;
	}

	RefreshVisibilityBasedAnimTickOption();

	RefreshMovementBase();

	RefreshInput(DeltaTime);

	RefreshLocomotionEarly();

	RefreshView(DeltaTime);

	RefreshRotationMode();

	RefreshLocomotion(DeltaTime);

	RefreshGait();

	RefreshGroundedRotation(DeltaTime);
	RefreshInAirRotation(DeltaTime);

	TryStartMantlingInAir();

	RefreshMantling();
	RefreshRagdolling(DeltaTime);
	RefreshRolling(DeltaTime);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshLocomotionLate(DeltaTime);

	if (!OwnerCharacter->GetMesh()->bRecentlyRendered &&
		OwnerCharacter->GetMesh()->VisibilityBasedAnimTickOption > EVisibilityBasedAnimTickOption::AlwaysTickPose)
	{
		AnimationInstance->MarkPendingUpdate();
	}
}

void UAlsComponent::PossessedBy(AController* NewController)
{
	// Super::PossessedBy(NewController);

	RefreshUsingAbsoluteRotation();
	RefreshVisibilityBasedAnimTickOption();

	// Enable view network smoothing on the listen server here because the remote role may not be valid yet during begin play.

	ViewState.NetworkSmoothing.bEnabled |= IsValid(Settings) && Settings->View.bEnableListenServerNetworkSmoothing &&
		IsNetMode(NM_ListenServer) && OwnerCharacter->GetRemoteRole() == ROLE_AutonomousProxy;
}

void UAlsComponent::Restart()
{
	// Super::Restart();

	ApplyDesiredStance();
}

void UAlsComponent::RefreshUsingAbsoluteRotation() const
{
	// Use absolute mesh rotation to be able to precisely synchronize character rotation
	// with animations by manually updating the mesh rotation from the animation instance.

	// This is necessary in cases where the character and the animation instance are ticking
	// at different frequencies, which leads to desynchronization of rotation animations
	// with the character rotation, as well as foot sliding when the foot lock is active.

	// To save performance, Use it only when it is really necessary, such as
	// when URO is enabled, or for autonomous proxies on the listen server.

	const auto bNotDedicatedServer{!IsNetMode(NM_DedicatedServer)};

	const auto bAutonomousProxyOnListenServer{IsNetMode(NM_ListenServer) && OwnerCharacter->GetRemoteRole() == ROLE_AutonomousProxy};

	const auto bNonLocallyControllerCharacterWithURO{
		OwnerCharacter->GetMesh()->ShouldUseUpdateRateOptimizations() && !IsValid(OwnerCharacter->GetInstigatorController<APlayerController>())
	};

	OwnerCharacter->GetMesh()->SetUsingAbsoluteRotation(bNotDedicatedServer && (bAutonomousProxyOnListenServer || bNonLocallyControllerCharacterWithURO));
}

void UAlsComponent::RefreshVisibilityBasedAnimTickOption() const
{
	const auto* CharacterCDO = GetOwner()->GetClass()->GetDefaultObject<ACharacter>();
	if (CharacterCDO == nullptr || CharacterCDO->GetMesh() == nullptr)
	{
		return;
	}
	const EVisibilityBasedAnimTickOption DefaultTickOption{CharacterCDO->GetMesh()->VisibilityBasedAnimTickOption};

	// Make sure that the pose is always ticked on the server when the character is controlled
	// by a remote client, otherwise some problems may arise (such as jitter when rolling).

	const auto TargetTickOption{
		IsNetMode(NM_Standalone) || OwnerCharacter->GetLocalRole() <= ROLE_AutonomousProxy || OwnerCharacter->GetRemoteRole() != ROLE_AutonomousProxy
			? EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered
			: EVisibilityBasedAnimTickOption::AlwaysTickPose
	};

	// Keep the default tick option, at least if the target tick option is not required by the plugin to work properly.

	OwnerCharacter->GetMesh()->VisibilityBasedAnimTickOption = TargetTickOption <= DefaultTickOption ? TargetTickOption : DefaultTickOption;
}

void UAlsComponent::RefreshMovementBase()
{
	if (OwnerCharacter->GetBasedMovement().MovementBase != MovementBase.Primitive || OwnerCharacter->GetBasedMovement().BoneName != MovementBase.BoneName)
	{
		MovementBase.Primitive = OwnerCharacter->GetBasedMovement().MovementBase;
		MovementBase.BoneName = OwnerCharacter->GetBasedMovement().BoneName;
		MovementBase.bBaseChanged = true;
	}
	else
	{
		MovementBase.bBaseChanged = false;
	}

	MovementBase.bHasRelativeLocation = OwnerCharacter->GetBasedMovement().HasRelativeLocation();
	MovementBase.bHasRelativeRotation = MovementBase.bHasRelativeLocation && OwnerCharacter->GetBasedMovement().bRelativeRotation;

	const auto PreviousRotation{MovementBase.Rotation};

	MovementBaseUtility::GetMovementBaseTransform(OwnerCharacter->GetBasedMovement().MovementBase, OwnerCharacter->GetBasedMovement().BoneName,
	                                              MovementBase.Location, MovementBase.Rotation);

	MovementBase.DeltaRotation = MovementBase.bHasRelativeLocation && !MovementBase.bBaseChanged
		                             ? (MovementBase.Rotation * PreviousRotation.Inverse()).Rotator()
		                             : FRotator::ZeroRotator;
}

void UAlsComponent::SetViewMode(const FGameplayTag& NewViewMode)
{
	if (ViewMode != NewViewMode)
	{
		ViewMode = NewViewMode;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ViewMode, this)

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetViewMode(NewViewMode);
		}
	}
}

void UAlsComponent::ServerSetViewMode_Implementation(const FGameplayTag& NewViewMode)
{
	SetViewMode(NewViewMode);
}

void UAlsComponent::OnMovementModeChanged(ACharacter* InCharacter, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	// Use the character movement mode to set the locomotion mode to the right value. This allows you to have a
	// custom set of movement modes but still use the functionality of the default character movement component.

	switch (OwnerCharacter->GetCharacterMovement()->MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		SetLocomotionMode(AlsLocomotionModeTags::Grounded);
		break;

	case MOVE_Falling:
		SetLocomotionMode(AlsLocomotionModeTags::InAir);
		break;

	default:
		SetLocomotionMode(FGameplayTag::EmptyTag);
		break;
	}
}

void UAlsComponent::SetLocomotionMode(const FGameplayTag& NewLocomotionMode)
{
	if (LocomotionMode != NewLocomotionMode)
	{
		const auto PreviousLocomotionMode{LocomotionMode};

		LocomotionMode = NewLocomotionMode;

		NotifyLocomotionModeChanged(PreviousLocomotionMode);
	}
}

void UAlsComponent::NotifyLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode)
{
	ApplyDesiredStance();

	if (LocomotionMode == AlsLocomotionModeTags::Grounded &&
		PreviousLocomotionMode == AlsLocomotionModeTags::InAir)
	{
		if (Settings->Ragdolling.bStartRagdollingOnLand &&
			LocomotionState.Velocity.Z <= -Settings->Ragdolling.RagdollingOnLandSpeedThreshold)
		{
			StartRagdolling();
		}
		else if (Settings->Rolling.bStartRollingOnLand &&
			LocomotionState.Velocity.Z <= -Settings->Rolling.RollingOnLandSpeedThreshold)
		{
			static constexpr auto PlayRate{1.3f};

			StartRolling(PlayRate, LocomotionState.bHasSpeed
				                       ? LocomotionState.VelocityYawAngle
				                       : UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(OwnerCharacter->GetActorRotation().Yaw)));
		}
		else
		{
			static constexpr auto HasInputBrakingFrictionFactor{0.5f};
			static constexpr auto NoInputBrakingFrictionFactor{3.0f};

			OwnerCharacter->GetCharacterMovement()->BrakingFrictionFactor = LocomotionState.bHasInput
				                                                                ? HasInputBrakingFrictionFactor
				                                                                : NoInputBrakingFrictionFactor;

			static constexpr auto ResetDelay{0.5f};

			OwnerCharacter->GetWorldTimerManager().SetTimer(BrakingFrictionFactorResetTimer,
			                                                FTimerDelegate::CreateWeakLambda(this, [this]
			                                                {
				                                                OwnerCharacter->GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
			                                                }), ResetDelay, false);

			// Block character rotation towards the last input direction after landing to
			// prevent legs from twisting into a spiral while the landing animation is playing.

			LocomotionState.bRotationTowardsLastInputDirectionBlocked = true;
		}
	}
	else if (LocomotionMode == AlsLocomotionModeTags::InAir &&
		LocomotionAction == AlsLocomotionActionTags::Rolling &&
		Settings->Rolling.bInterruptRollingWhenInAir)
	{
		// If the character is currently rolling, then enable ragdolling.

		StartRagdolling();
	}

	OnLocomotionModeChanged(PreviousLocomotionMode);
}

void UAlsComponent::OnLocomotionModeChanged_Implementation(const FGameplayTag& PreviousLocomotionMode)
{
	OnLocomotionModeChangedEvent.Broadcast(PreviousLocomotionMode);
}

void UAlsComponent::SetDesiredAiming(const bool bNewDesiredAiming)
{
	if (bDesiredAiming != bNewDesiredAiming)
	{
		bDesiredAiming = bNewDesiredAiming;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bDesiredAiming, this)

		OnDesiredAimingChanged(!bDesiredAiming);

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetDesiredAiming(bDesiredAiming);
		}
	}
}

void UAlsComponent::OnReplicated_DesiredAiming(const bool bPreviousDesiredAiming)
{
	OnDesiredAimingChanged(bPreviousDesiredAiming);
}

void UAlsComponent::OnDesiredAimingChanged_Implementation(const bool bPreviousDesiredAiming)
{
}

void UAlsComponent::ServerSetDesiredAiming_Implementation(const bool bNewAiming)
{
	SetDesiredAiming(bNewAiming);
}

void UAlsComponent::SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode)
{
	if (DesiredRotationMode != NewDesiredRotationMode)
	{
		DesiredRotationMode = NewDesiredRotationMode;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DesiredRotationMode, this)

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetDesiredRotationMode(DesiredRotationMode);
		}
	}
}

void UAlsComponent::ServerSetDesiredRotationMode_Implementation(const FGameplayTag& NewDesiredRotationMode)
{
	SetDesiredRotationMode(NewDesiredRotationMode);
}

void UAlsComponent::SetRotationMode(const FGameplayTag& NewRotationMode)
{
	AlsCharacterMovement->SetRotationMode(NewRotationMode);

	if (RotationMode != NewRotationMode)
	{
		const auto PreviousRotationMode{RotationMode};

		RotationMode = NewRotationMode;

		OnRotationModeChanged(PreviousRotationMode);
	}
}

void UAlsComponent::OnRotationModeChanged_Implementation(const FGameplayTag& PreviousRotationMode)
{
}

void UAlsComponent::RefreshRotationMode()
{
	const auto bSprinting{Gait == AlsGaitTags::Sprinting};
	const auto bAiming{bDesiredAiming || DesiredRotationMode == AlsRotationModeTags::Aiming};

	if (ViewMode == AlsViewModeTags::FirstPerson)
	{
		if (LocomotionMode == AlsLocomotionModeTags::InAir)
		{
			if (bAiming && Settings->bAllowAimingWhenInAir)
			{
				SetRotationMode(AlsRotationModeTags::Aiming);
			}
			else
			{
				SetRotationMode(AlsRotationModeTags::ViewDirection);
			}

			return;
		}

		// Grounded and other locomotion modes.

		if (bAiming && (!bSprinting || !Settings->bSprintHasPriorityOverAiming))
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}

		return;
	}

	// Third person and other view modes.

	if (LocomotionMode == AlsLocomotionModeTags::InAir)
	{
		if (bAiming && Settings->bAllowAimingWhenInAir)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}
		else
		{
			SetRotationMode(DesiredRotationMode);
		}

		return;
	}

	// Grounded and other locomotion modes.

	if (bSprinting)
	{
		if (bAiming && !Settings->bSprintHasPriorityOverAiming)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else if (Settings->bRotateToVelocityWhenSprinting)
		{
			SetRotationMode(AlsRotationModeTags::VelocityDirection);
		}
		else if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}
		else
		{
			SetRotationMode(DesiredRotationMode);
		}
	}
	else // Not sprinting.
	{
		if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else
		{
			SetRotationMode(DesiredRotationMode);
		}
	}
}

void UAlsComponent::SetDesiredStance(const FGameplayTag& NewDesiredStance)
{
	if (DesiredStance != NewDesiredStance)
	{
		DesiredStance = NewDesiredStance;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DesiredStance, this)

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetDesiredStance(DesiredStance);
		}

		ApplyDesiredStance();
	}
}

void UAlsComponent::ServerSetDesiredStance_Implementation(const FGameplayTag& NewDesiredStance)
{
	SetDesiredStance(NewDesiredStance);
}

void UAlsComponent::ApplyDesiredStance()
{
	if (!LocomotionAction.IsValid())
	{
		if (LocomotionMode == AlsLocomotionModeTags::Grounded)
		{
			if (DesiredStance == AlsStanceTags::Standing)
			{
				OwnerCharacter->UnCrouch();
			}
			else if (DesiredStance == AlsStanceTags::Crouching)
			{
				OwnerCharacter->Crouch();
			}
		}
		else if (LocomotionMode == AlsLocomotionModeTags::InAir)
		{
			OwnerCharacter->UnCrouch();
		}
	}
	else if (LocomotionAction == AlsLocomotionActionTags::Rolling && Settings->Rolling.bCrouchOnStart)
	{
		OwnerCharacter->Crouch();
	}
}

// bool UAlsComponent::CanCrouch() const
// {
// 	// This allows to execute the ACharacter::Crouch() function properly when bIsCrouched is true.
// 	return OwnerCharacter->bIsCrouched;
// }

void UAlsComponent::OnStartCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	auto* PredictionData{OwnerCharacter->GetCharacterMovement()->GetPredictionData_Client_Character()};

	if (PredictionData != nullptr && OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy &&
		ScaledHalfHeightAdjust > 0.0f && OwnerCharacter->IsPlayingNetworkedRootMotionMontage())
	{
		// The code below essentially undoes the changes that will be made later at the end of the UCharacterMovementComponent::Crouch()
		// function because they literally break network smoothing when crouching while the root motion montage is playing, causing the
		// mesh to take an incorrect location for a while.

		// TODO Check the need for this fix in future engine versions.

		PredictionData->MeshTranslationOffset.Z += ScaledHalfHeightAdjust;
		PredictionData->OriginalMeshTranslationOffset = PredictionData->MeshTranslationOffset;
	}
}

void UAlsComponent::OnPostStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	SetStance(AlsStanceTags::Crouching);
}

void UAlsComponent::OnEndCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	auto* PredictionData{OwnerCharacter->GetCharacterMovement()->GetPredictionData_Client_Character()};

	if (PredictionData != nullptr && OwnerCharacter->GetLocalRole() <= ROLE_SimulatedProxy &&
		ScaledHalfHeightAdjust > 0.0f && OwnerCharacter->IsPlayingNetworkedRootMotionMontage())
	{
		// Same fix as in AAlsCharacter::OnStartCrouch().

		PredictionData->MeshTranslationOffset.Z -= ScaledHalfHeightAdjust;
		PredictionData->OriginalMeshTranslationOffset = PredictionData->MeshTranslationOffset;
	}
}

void UAlsComponent::OnPostEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	SetStance(AlsStanceTags::Standing);
}

void UAlsComponent::SetStance(const FGameplayTag& NewStance)
{
	AlsCharacterMovement->SetStance(NewStance);

	if (Stance != NewStance)
	{
		const auto PreviousStance{Stance};

		Stance = NewStance;

		OnStanceChanged(PreviousStance);
	}
}

void UAlsComponent::OnStanceChanged_Implementation(const FGameplayTag& PreviousStance)
{
}

void UAlsComponent::SetDesiredGait(const FGameplayTag& NewDesiredGait)
{
	if (DesiredGait != NewDesiredGait)
	{
		DesiredGait = NewDesiredGait;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DesiredGait, this)

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetDesiredGait(DesiredGait);
		}
	}
}

void UAlsComponent::ServerSetDesiredGait_Implementation(const FGameplayTag& NewDesiredGait)
{
	SetDesiredGait(NewDesiredGait);
}

void UAlsComponent::SetGait(const FGameplayTag& NewGait)
{
	if (Gait != NewGait)
	{
		const auto PreviousGait{Gait};

		Gait = NewGait;

		OnGaitChanged(PreviousGait);
	}
}

void UAlsComponent::OnGaitChanged_Implementation(const FGameplayTag& PreviousGait)
{
}

void UAlsComponent::RefreshGait()
{
	if (LocomotionMode != AlsLocomotionModeTags::Grounded)
	{
		return;
	}

	const auto MaxAllowedGait{CalculateMaxAllowedGait()};

	// Update the character max walk speed to the configured speeds based on the currently max allowed gait.

	AlsCharacterMovement->SetMaxAllowedGait(MaxAllowedGait);

	SetGait(CalculateActualGait(MaxAllowedGait));
}

FGameplayTag UAlsComponent::CalculateMaxAllowedGait() const
{
	// Calculate the max allowed gait. This represents the maximum gait the character is currently allowed
	// to be in and can be determined by the desired gait, the rotation mode, the stance, etc. For example,
	// if you wanted to force the character into a walking state while indoors, this could be done here.

	if (DesiredGait != AlsGaitTags::Sprinting)
	{
		return DesiredGait;
	}

	if (CanSprint())
	{
		return AlsGaitTags::Sprinting;
	}

	return AlsGaitTags::Running;
}

FGameplayTag UAlsComponent::CalculateActualGait(const FGameplayTag& MaxAllowedGait) const
{
	// Calculate the new gait. This is calculated by the actual movement of the character and so it can be
	// different from the desired gait or max allowed gait. For instance, if the max allowed gait becomes
	// walking, the new gait will still be running until the character decelerates to the walking speed.

	if (LocomotionState.Speed < AlsCharacterMovement->GetGaitSettings().WalkSpeed + 10.0f)
	{
		return AlsGaitTags::Walking;
	}

	if (LocomotionState.Speed < AlsCharacterMovement->GetGaitSettings().RunSpeed + 10.0f || MaxAllowedGait != AlsGaitTags::Sprinting)
	{
		return AlsGaitTags::Running;
	}

	return AlsGaitTags::Sprinting;
}

bool UAlsComponent::CanSprint() const
{
	// Determine if the character can sprint based on the rotation mode and input direction.
	// If the character is in view direction rotation mode, only allow sprinting if there is
	// input and if the input direction is aligned with the view direction within 50 degrees.

	if (!LocomotionState.bHasInput || Stance != AlsStanceTags::Standing ||
		(RotationMode == AlsRotationModeTags::Aiming && !Settings->bSprintHasPriorityOverAiming))
	{
		return false;
	}

	if (ViewMode != AlsViewModeTags::FirstPerson &&
		(DesiredRotationMode == AlsRotationModeTags::VelocityDirection || Settings->bRotateToVelocityWhenSprinting))
	{
		return true;
	}

	static constexpr auto ViewRelativeAngleThreshold{50.0f};

	if (FMath::Abs(FRotator3f::NormalizeAxis(UE_REAL_TO_FLOAT(
		LocomotionState.InputYawAngle - ViewState.Rotation.Yaw))) < ViewRelativeAngleThreshold)
	{
		return true;
	}

	return false;
}

void UAlsComponent::SetOverlayMode(const FGameplayTag& NewOverlayMode)
{
	if (OverlayMode != NewOverlayMode)
	{
		const auto PreviousOverlayMode{OverlayMode};

		OverlayMode = NewOverlayMode;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, OverlayMode, this)

		OnOverlayModeChanged(PreviousOverlayMode);

		if (OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetOverlayMode(OverlayMode);
		}
	}
}

void UAlsComponent::ServerSetOverlayMode_Implementation(const FGameplayTag& NewOverlayMode)
{
	SetOverlayMode(NewOverlayMode);
}

void UAlsComponent::OnReplicated_OverlayMode(const FGameplayTag& PreviousOverlayMode)
{
	OnOverlayModeChanged(PreviousOverlayMode);
}

void UAlsComponent::OnOverlayModeChanged_Implementation(const FGameplayTag& PreviousOverlayMode)
{
	OnOverlayModeChangedEvent.Broadcast(PreviousOverlayMode);
}

void UAlsComponent::SetLocomotionAction(const FGameplayTag& NewLocomotionAction)
{
	if (LocomotionAction != NewLocomotionAction)
	{
		const auto PreviousLocomotionAction{LocomotionAction};

		LocomotionAction = NewLocomotionAction;

		NotifyLocomotionActionChanged(PreviousLocomotionAction);
	}
}

void UAlsComponent::NotifyLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction)
{
	ApplyDesiredStance();

	OnLocomotionActionChanged(PreviousLocomotionAction);
}

void UAlsComponent::OnLocomotionActionChanged_Implementation(const FGameplayTag& PreviousLocomotionAction)
{
	OnLocomotionActionChangedEvent.Broadcast(PreviousLocomotionAction);
}

FRotator UAlsComponent::GetViewRotation() const
{
	return ViewState.Rotation;
}

void UAlsComponent::SetInputDirection(FVector NewInputDirection)
{
	NewInputDirection = NewInputDirection.GetSafeNormal();

	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(ThisClass, InputDirection, NewInputDirection, this);
}

void UAlsComponent::RefreshInput(const float DeltaTime)
{
	if (OwnerCharacter->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		SetInputDirection(OwnerCharacter->GetCharacterMovement()->GetCurrentAcceleration() / OwnerCharacter->GetCharacterMovement()->GetMaxAcceleration());
	}

	LocomotionState.bHasInput = InputDirection.SizeSquared() > UE_KINDA_SMALL_NUMBER;

	if (LocomotionState.bHasInput)
	{
		LocomotionState.InputYawAngle = UE_REAL_TO_FLOAT(UAlsMath::DirectionToAngleXY(InputDirection));
	}
}

void UAlsComponent::SetReplicatedViewRotation(const FRotator& NewViewRotation)
{
	if (ReplicatedViewRotation != NewViewRotation)
	{
		ReplicatedViewRotation = NewViewRotation;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedViewRotation, this)

		// The character movement component already sends the view rotation to the
		// server if the movement is replicated, so we don't have to do it ourselves.

		if (!OwnerCharacter->IsReplicatingMovement() && OwnerCharacter->GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetReplicatedViewRotation(ReplicatedViewRotation);
		}
	}
}

void UAlsComponent::ServerSetReplicatedViewRotation_Implementation(const FRotator& NewViewRotation)
{
	SetReplicatedViewRotation(NewViewRotation);
}

void UAlsComponent::OnReplicated_ReplicatedViewRotation()
{
	CorrectViewNetworkSmoothing(ReplicatedViewRotation);
}

void UAlsComponent::CorrectViewNetworkSmoothing(const FRotator& NewViewRotation)
{
	// Based on UCharacterMovementComponent::SmoothCorrection().

	ReplicatedViewRotation = NewViewRotation;
	ReplicatedViewRotation.Normalize();

	auto& NetworkSmoothing{ViewState.NetworkSmoothing};

	if (!NetworkSmoothing.bEnabled)
	{
		NetworkSmoothing.InitialRotation = ReplicatedViewRotation;
		NetworkSmoothing.Rotation = ReplicatedViewRotation;
		return;
	}

	const auto bListenServer{IsNetMode(NM_ListenServer)};

	const auto NewNetworkSmoothingServerTime{
		bListenServer
			? OwnerCharacter->GetCharacterMovement()->GetServerLastTransformUpdateTimeStamp()
			: OwnerCharacter->GetReplicatedServerLastTransformUpdateTimeStamp()
	};

	if (NewNetworkSmoothingServerTime <= 0.0f)
	{
		return;
	}

	NetworkSmoothing.InitialRotation = NetworkSmoothing.Rotation;

	// Using server time lets us know how much time elapsed, regardless of packet lag variance.

	const auto ServerDeltaTime{NewNetworkSmoothingServerTime - NetworkSmoothing.ServerTime};

	NetworkSmoothing.ServerTime = NewNetworkSmoothingServerTime;

	// Don't let the client fall too far behind or run ahead of new server time.

	const auto MaxServerDeltaTime{GetDefault<AGameNetworkManager>()->MaxClientSmoothingDeltaTime};

	const auto MinServerDeltaTime{
		FMath::Min(MaxServerDeltaTime, bListenServer
			                               ? OwnerCharacter->GetCharacterMovement()->ListenServerNetworkSimulatedSmoothLocationTime
			                               : OwnerCharacter->GetCharacterMovement()->NetworkSimulatedSmoothLocationTime)
	};

	// Calculate how far behind we can be after receiving a new server time.

	const auto MinClientDeltaTime{FMath::Clamp(ServerDeltaTime * 1.25f, MinServerDeltaTime, MaxServerDeltaTime)};

	NetworkSmoothing.ClientTime = FMath::Clamp(NetworkSmoothing.ClientTime,
	                                           NetworkSmoothing.ServerTime - MinClientDeltaTime,
	                                           NetworkSmoothing.ServerTime);

	// Compute actual delta between new server time and client simulation.

	NetworkSmoothing.Duration = NetworkSmoothing.ServerTime - NetworkSmoothing.ClientTime;
}

void UAlsComponent::RefreshView(const float DeltaTime)
{
	if (MovementBase.bHasRelativeRotation)
	{
		// Offset the rotations to keep them relative to the movement base.

		ReplicatedViewRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		ReplicatedViewRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		ReplicatedViewRotation.Normalize();

		ViewState.Rotation.Pitch += MovementBase.DeltaRotation.Pitch;
		ViewState.Rotation.Yaw += MovementBase.DeltaRotation.Yaw;
		ViewState.Rotation.Normalize();
	}

	ViewState.PreviousYawAngle = UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw);

	if ((OwnerCharacter->IsReplicatingMovement() && OwnerCharacter->GetLocalRole() >= ROLE_AutonomousProxy) || OwnerCharacter->IsLocallyControlled())
	{
		SetReplicatedViewRotation(OwnerCharacter->GetControlRotation().GetNormalized());
	}

	RefreshViewNetworkSmoothing(DeltaTime);

	ViewState.Rotation = ViewState.NetworkSmoothing.Rotation;

	// Set the yaw speed by comparing the current and previous view yaw angle, divided by
	// delta seconds. This represents the speed the camera is rotating from left to right.

	ViewState.YawSpeed = FMath::Abs(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw) - ViewState.PreviousYawAngle) / DeltaTime;
}

void UAlsComponent::RefreshViewNetworkSmoothing(const float DeltaTime)
{
	// Based on UCharacterMovementComponent::SmoothClientPosition_Interpolate()
	// and UCharacterMovementComponent::SmoothClientPosition_UpdateVisuals().

	auto& NetworkSmoothing{ViewState.NetworkSmoothing};

	if (!NetworkSmoothing.bEnabled ||
		NetworkSmoothing.ClientTime >= NetworkSmoothing.ServerTime ||
		NetworkSmoothing.Duration <= UE_SMALL_NUMBER)
	{
		NetworkSmoothing.InitialRotation = ReplicatedViewRotation;
		NetworkSmoothing.Rotation = ReplicatedViewRotation;
		return;
	}

	if (MovementBase.bHasRelativeRotation)
	{
		// Offset the rotations to keep them relative to the movement base.

		NetworkSmoothing.InitialRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NetworkSmoothing.InitialRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NetworkSmoothing.InitialRotation.Normalize();

		NetworkSmoothing.Rotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NetworkSmoothing.Rotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NetworkSmoothing.Rotation.Normalize();
	}

	NetworkSmoothing.ClientTime += DeltaTime;

	const auto InterpolationAmount{
		UAlsMath::Clamp01(1.0f - (NetworkSmoothing.ServerTime - NetworkSmoothing.ClientTime) / NetworkSmoothing.Duration)
	};

	if (!FAnimWeight::IsFullWeight(InterpolationAmount))
	{
		NetworkSmoothing.Rotation = UAlsMath::LerpRotator(NetworkSmoothing.InitialRotation, ReplicatedViewRotation, InterpolationAmount);
	}
	else
	{
		NetworkSmoothing.ClientTime = NetworkSmoothing.ServerTime;
		NetworkSmoothing.Rotation = ReplicatedViewRotation;
	}
}

void UAlsComponent::SetDesiredVelocityYawAngle(const float NewDesiredVelocityYawAngle)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(ThisClass, DesiredVelocityYawAngle, NewDesiredVelocityYawAngle, this);
}

void UAlsComponent::RefreshLocomotionLocationAndRotation()
{
	const auto& ActorTransform{OwnerCharacter->GetActorTransform()};

	// If network smoothing is disabled, then return regular actor transform.

	if (OwnerCharacter->GetCharacterMovement()->NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
	{
		LocomotionState.Location = ActorTransform.GetLocation();
		LocomotionState.RotationQuaternion = ActorTransform.GetRotation();
		LocomotionState.Rotation = GetOwner()->GetActorRotation();
	}
	else if (OwnerCharacter->GetMesh()->IsUsingAbsoluteRotation())
	{
		LocomotionState.Location = ActorTransform.TransformPosition(OwnerCharacter->GetMesh()->GetRelativeLocation() - OwnerCharacter->GetBaseTranslationOffset());
		LocomotionState.RotationQuaternion = ActorTransform.GetRotation();
		LocomotionState.Rotation = GetOwner()->GetActorRotation();
	}
	else
	{
		const auto SmoothTransform{
			ActorTransform * FTransform{
				OwnerCharacter->GetMesh()->GetRelativeRotationCache().RotatorToQuat(OwnerCharacter->GetMesh()->GetRelativeRotation()) * OwnerCharacter->GetBaseRotationOffset().Inverse(),
				OwnerCharacter->GetMesh()->GetRelativeLocation() - OwnerCharacter->GetBaseTranslationOffset()
			}
		};

		LocomotionState.Location = SmoothTransform.GetLocation();
		LocomotionState.RotationQuaternion = SmoothTransform.GetRotation();
		LocomotionState.Rotation = LocomotionState.RotationQuaternion.Rotator();
	}
}

void UAlsComponent::RefreshLocomotionEarly()
{
	if (MovementBase.bHasRelativeRotation)
	{
		// Offset the rotations (the actor's rotation too) to keep them relative to the movement base.

		LocomotionState.TargetYawAngle = FRotator3f::NormalizeAxis(LocomotionState.TargetYawAngle +
			MovementBase.DeltaRotation.Yaw);

		LocomotionState.ViewRelativeTargetYawAngle = FRotator3f::NormalizeAxis(LocomotionState.ViewRelativeTargetYawAngle +
			MovementBase.DeltaRotation.Yaw);

		LocomotionState.SmoothTargetYawAngle = FRotator3f::NormalizeAxis(LocomotionState.SmoothTargetYawAngle +
			MovementBase.DeltaRotation.Yaw);

		auto NewRotation{GetOwner()->GetActorRotation()};
		NewRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NewRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NewRotation.Normalize();

		GetOwner()->SetActorRotation(NewRotation);
	}

	RefreshLocomotionLocationAndRotation();

	LocomotionState.PreviousVelocity = LocomotionState.Velocity;
	LocomotionState.PreviousYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);
}

void UAlsComponent::RefreshLocomotion(const float DeltaTime)
{
	LocomotionState.PreviousVelocity = LocomotionState.Velocity;
	LocomotionState.PreviousYawAngle = UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw);

	if (OwnerCharacter->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		SetInputDirection(OwnerCharacter->GetCharacterMovement()->GetCurrentAcceleration() / OwnerCharacter->GetCharacterMovement()->GetMaxAcceleration());
	}

	// If the character has the input, update the input yaw angle.

	LocomotionState.bHasInput = InputDirection.SizeSquared() > UE_KINDA_SMALL_NUMBER;

	if (LocomotionState.bHasInput)
	{
		LocomotionState.InputYawAngle = UE_REAL_TO_FLOAT(UAlsMath::DirectionToAngleXY(InputDirection));
	}

	LocomotionState.Velocity = OwnerCharacter->GetVelocity();

	// Determine if the character is moving by getting its speed. The speed equals the length
	// of the horizontal velocity, so it does not take vertical movement into account. If the
	// character is moving, update the last velocity rotation. This value is saved because it might
	// be useful to know the last orientation of a movement even after the character has stopped.

	LocomotionState.Speed = UE_REAL_TO_FLOAT(LocomotionState.Velocity.Size2D());
	LocomotionState.bHasSpeed = LocomotionState.Speed >= 1.0f;

	if (LocomotionState.bHasSpeed)
	{
		LocomotionState.VelocityYawAngle = UE_REAL_TO_FLOAT(UAlsMath::DirectionToAngleXY(LocomotionState.Velocity));
	}

	LocomotionState.Acceleration = (LocomotionState.Velocity - LocomotionState.PreviousVelocity) / DeltaTime;

	// Character is moving if has speed and current acceleration, or if the speed is greater than the moving speed threshold.

	// ReSharper disable once CppRedundantParentheses
	LocomotionState.bMoving = (LocomotionState.bHasInput && LocomotionState.bHasSpeed) ||
		LocomotionState.Speed > Settings->MovingSpeedThreshold;
}

void UAlsComponent::RefreshLocomotionLate(const float DeltaTime)
{
	if (!LocomotionMode.IsValid() || LocomotionAction.IsValid())
	{
		RefreshLocomotionLocationAndRotation();
		RefreshTargetYawAngleUsingLocomotionRotation();
	}

	LocomotionState.YawSpeed = FRotator3f::NormalizeAxis(UE_REAL_TO_FLOAT(
		LocomotionState.Rotation.Yaw - LocomotionState.PreviousYawAngle)) / DeltaTime;
}

bool UAlsComponent::CanJump() const
{
	return Stance == AlsStanceTags::Standing && !LocomotionAction.IsValid() &&
		LocomotionMode == AlsLocomotionModeTags::Grounded;
}

void UAlsComponent::OnJumped()
{
	if (OwnerCharacter->IsLocallyControlled())
	{
		OnJumpedNetworked();
	}

	if (OwnerCharacter->GetLocalRole() >= ROLE_Authority)
	{
		MulticastOnJumpedNetworked();
	}
}

void UAlsComponent::MulticastOnJumpedNetworked_Implementation()
{
	if (!OwnerCharacter->IsLocallyControlled())
	{
		OnJumpedNetworked();
	}
}

void UAlsComponent::OnJumpedNetworked()
{
	if (AnimationInstance.IsValid())
	{
		AnimationInstance->Jump();
	}
}

void UAlsComponent::CharacterMovement_OnPhysicsRotation(const float DeltaTime)
{
	RefreshRollingPhysics(DeltaTime);
}

void UAlsComponent::RefreshGroundedRotation(const float DeltaTime)
{
	if (LocomotionAction.IsValid() || LocomotionMode != AlsLocomotionModeTags::Grounded)
	{
		return;
	}

	if (OwnerCharacter->HasAnyRootMotion())
	{
		RefreshTargetYawAngleUsingLocomotionRotation();
		return;
	}

	if (!LocomotionState.bMoving)
	{
		// Not moving.

		ApplyRotationYawSpeed(DeltaTime);

		if (RefreshCustomGroundedNotMovingRotation(DeltaTime))
		{
			return;
		}

		if (RotationMode == AlsRotationModeTags::Aiming || ViewMode == AlsViewModeTags::FirstPerson)
		{
			RefreshGroundedNotMovingAimingRotation(DeltaTime);
			return;
		}

		if (RotationMode == AlsRotationModeTags::VelocityDirection)
		{
			// Rotate to the last target yaw angle when not moving (relative to the movement base or not).

			const auto TargetYawAngle{
				MovementBase.bHasRelativeLocation && !MovementBase.bHasRelativeRotation &&
				Settings->bInheritMovementBaseRotationInVelocityDirectionRotationMode
					? FRotator3f::NormalizeAxis(LocomotionState.TargetYawAngle + MovementBase.DeltaRotation.Yaw)
					: LocomotionState.TargetYawAngle
			};

			static constexpr auto RotationInterpolationSpeed{12.0f};
			static constexpr auto TargetYawAngleRotationSpeed{800.0f};

			RefreshRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationSpeed, TargetYawAngleRotationSpeed);
			return;
		}

		RefreshTargetYawAngleUsingLocomotionRotation();
		return;
	}

	// Moving.

	if (RefreshCustomGroundedMovingRotation(DeltaTime))
	{
		return;
	}

	if (RotationMode == AlsRotationModeTags::VelocityDirection &&
		(LocomotionState.bHasInput || !LocomotionState.bRotationTowardsLastInputDirectionBlocked))
	{
		LocomotionState.bRotationTowardsLastInputDirectionBlocked = false;

		static constexpr auto TargetYawAngleRotationSpeed{800.0f};

		RefreshRotationExtraSmooth(LocomotionState.VelocityYawAngle, DeltaTime,
		                           CalculateRotationInterpolationSpeed(), TargetYawAngleRotationSpeed);
		return;
	}

	if (RotationMode == AlsRotationModeTags::ViewDirection)
	{
		const auto TargetYawAngle{
			Gait == AlsGaitTags::Sprinting
				? LocomotionState.VelocityYawAngle
				: UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw) +
				OwnerCharacter->GetMesh()->GetAnimInstance()->GetCurveValue(UAlsConstants::RotationYawOffsetCurveName())
		};

		static constexpr auto TargetYawAngleRotationSpeed{500.0f};

		RefreshRotationExtraSmooth(TargetYawAngle, DeltaTime, CalculateRotationInterpolationSpeed(), TargetYawAngleRotationSpeed);
		return;
	}

	if (RotationMode == AlsRotationModeTags::Aiming)
	{
		RefreshGroundedMovingAimingRotation(DeltaTime);
		return;
	}

	RefreshTargetYawAngleUsingLocomotionRotation();
}

bool UAlsComponent::RefreshCustomGroundedMovingRotation(const float DeltaTime)
{
	return false;
}

bool UAlsComponent::RefreshCustomGroundedNotMovingRotation(const float DeltaTime)
{
	return false;
}

void UAlsComponent::RefreshGroundedMovingAimingRotation(const float DeltaTime)
{
	static constexpr auto RotationInterpolationSpeed{20.0f};
	static constexpr auto TargetYawAngleRotationSpeed{1000.0f};

	RefreshRotationExtraSmooth(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw), DeltaTime,
	                           RotationInterpolationSpeed, TargetYawAngleRotationSpeed);
}

void UAlsComponent::RefreshGroundedNotMovingAimingRotation(const float DeltaTime)
{
	static constexpr auto RotationInterpolationSpeed{20.0f};

	if (LocomotionState.bHasInput)
	{
		static constexpr auto TargetYawAngleRotationSpeed{1000.0f};

		RefreshRotationExtraSmooth(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw), DeltaTime,
		                           RotationInterpolationSpeed, TargetYawAngleRotationSpeed);
		return;
	}

	// Prevent the character from rotating past a certain angle.

	auto ViewRelativeYawAngle{FRotator3f::NormalizeAxis(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - LocomotionState.Rotation.Yaw))};

	static constexpr auto ViewRelativeYawAngleThreshold{70.0f};

	if (FMath::Abs(ViewRelativeYawAngle) > ViewRelativeYawAngleThreshold)
	{
		if (ViewRelativeYawAngle > 180.0f - UAlsMath::CounterClockwiseRotationAngleThreshold)
		{
			ViewRelativeYawAngle -= 360.0f;
		}

		RefreshRotation(FRotator3f::NormalizeAxis(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw) +
			                (ViewRelativeYawAngle >= 0.0f
				                 ? -ViewRelativeYawAngleThreshold
				                 : ViewRelativeYawAngleThreshold)),
		                DeltaTime, RotationInterpolationSpeed);
	}
	else
	{
		RefreshTargetYawAngleUsingLocomotionRotation();
	}
}

float UAlsComponent::CalculateRotationInterpolationSpeed() const
{
	// Calculate the rotation speed by using the rotation speed curve in the movement gait settings. Using
	// the curve in conjunction with the gait amount gives you a high level of control over the rotation
	// rates for each speed. Increase the speed if the camera is rotating quickly for more responsive rotation.

	static constexpr auto ReferenceViewYawSpeed{300.0f};
	static constexpr auto InterpolationSpeedMultiplier{3.0f};

	return AlsCharacterMovement->GetGaitSettings().RotationInterpolationSpeedCurve
	                           ->GetFloatValue(AlsCharacterMovement->CalculateGaitAmount()) *
		UAlsMath::LerpClamped(1.0f, InterpolationSpeedMultiplier, ViewState.YawSpeed / ReferenceViewYawSpeed);
}

void UAlsComponent::ApplyRotationYawSpeed(const float DeltaTime)
{
	const auto DeltaYawAngle{OwnerCharacter->GetMesh()->GetAnimInstance()->GetCurveValue(UAlsConstants::RotationYawSpeedCurveName()) * DeltaTime};
	if (FMath::Abs(DeltaYawAngle) > UE_SMALL_NUMBER)
	{
		auto NewRotation{OwnerCharacter->GetActorRotation()};
		NewRotation.Yaw += DeltaYawAngle;

		OwnerCharacter->SetActorRotation(NewRotation);

		RefreshLocomotionLocationAndRotation();
		RefreshTargetYawAngleUsingLocomotionRotation();
	}
}

void UAlsComponent::RefreshInAirRotation(const float DeltaTime)
{
	if (LocomotionAction.IsValid() || LocomotionMode != AlsLocomotionModeTags::InAir)
	{
		return;
	}

	if (RefreshCustomInAirRotation(DeltaTime))
	{
		return;
	}

	static constexpr auto RotationInterpolationSpeed{5.0f};

	if (RotationMode == AlsRotationModeTags::VelocityDirection || RotationMode == AlsRotationModeTags::ViewDirection)
	{
		switch (Settings->InAirRotationMode)
		{
		case EAlsInAirRotationMode::RotateToVelocityOnJump:
			if (LocomotionState.bMoving)
			{
				RefreshRotation(LocomotionState.VelocityYawAngle, DeltaTime, RotationInterpolationSpeed);
			}
			else
			{
				RefreshTargetYawAngleUsingLocomotionRotation();
			}
			break;

		case EAlsInAirRotationMode::KeepRelativeRotation:
			RefreshRotation(
				FRotator3f::NormalizeAxis(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw) - LocomotionState.ViewRelativeTargetYawAngle),
				DeltaTime, RotationInterpolationSpeed);
			break;

		default:
			RefreshTargetYawAngleUsingLocomotionRotation();
			break;
		}
	}
	else if (RotationMode == AlsRotationModeTags::Aiming)
	{
		RefreshInAirAimingRotation(DeltaTime);
	}
	else
	{
		RefreshTargetYawAngleUsingLocomotionRotation();
	}
}

bool UAlsComponent::RefreshCustomInAirRotation(const float DeltaTime)
{
	return false;
}

void UAlsComponent::RefreshInAirAimingRotation(const float DeltaTime)
{
	static constexpr auto RotationInterpolationSpeed{15.0f};

	RefreshRotation(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw), DeltaTime, RotationInterpolationSpeed);
}

void UAlsComponent::RefreshRotation(const float TargetYawAngle, const float DeltaTime, const float RotationInterpolationSpeed)
{
	RefreshTargetYawAngle(TargetYawAngle);

	auto NewRotation{OwnerCharacter->GetActorRotation()};
	NewRotation.Yaw = UAlsMath::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(NewRotation.Yaw)),
	                                                  TargetYawAngle, DeltaTime, RotationInterpolationSpeed);

	OwnerCharacter->SetActorRotation(NewRotation);

	RefreshLocomotionLocationAndRotation();
}

void UAlsComponent::RefreshRotationExtraSmooth(const float TargetYawAngle, const float DeltaTime,
                                               const float RotationInterpolationSpeed, const float TargetYawAngleRotationSpeed)
{
	LocomotionState.TargetYawAngle = TargetYawAngle;

	RefreshViewRelativeTargetYawAngle();

	// Interpolate target yaw angle for extra smooth rotation.

	LocomotionState.SmoothTargetYawAngle = UAlsMath::InterpolateAngleConstant(LocomotionState.SmoothTargetYawAngle, TargetYawAngle,
	                                                                          DeltaTime, TargetYawAngleRotationSpeed);

	auto NewRotation{OwnerCharacter->GetActorRotation()};
	NewRotation.Yaw = UAlsMath::ExponentialDecayAngle(UE_REAL_TO_FLOAT(FRotator::NormalizeAxis(NewRotation.Yaw)),
	                                                  LocomotionState.SmoothTargetYawAngle, DeltaTime, RotationInterpolationSpeed);

	OwnerCharacter->SetActorRotation(NewRotation);

	RefreshLocomotionLocationAndRotation();
}

void UAlsComponent::RefreshRotationInstant(const float TargetYawAngle, const ETeleportType Teleport)
{
	RefreshTargetYawAngle(TargetYawAngle);

	auto NewRotation{OwnerCharacter->GetActorRotation()};
	NewRotation.Yaw = TargetYawAngle;

	OwnerCharacter->SetActorRotation(NewRotation, Teleport);

	RefreshLocomotionLocationAndRotation();
}

void UAlsComponent::RefreshTargetYawAngleUsingLocomotionRotation()
{
	RefreshTargetYawAngle(UE_REAL_TO_FLOAT(LocomotionState.Rotation.Yaw));
}

void UAlsComponent::RefreshTargetYawAngle(const float TargetYawAngle)
{
	LocomotionState.TargetYawAngle = TargetYawAngle;

	RefreshViewRelativeTargetYawAngle();

	LocomotionState.SmoothTargetYawAngle = TargetYawAngle;
}

void UAlsComponent::RefreshViewRelativeTargetYawAngle()
{
	LocomotionState.ViewRelativeTargetYawAngle =
		FRotator3f::NormalizeAxis(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw) - LocomotionState.TargetYawAngle);
}
