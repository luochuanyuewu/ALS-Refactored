#include "AlsSimpleCharacterExample.h"

#include "AlsCameraComponent.h"
#include "AlsComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsSimpleCharacterExample)

AAlsSimpleCharacterExample::AAlsSimpleCharacterExample(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Camera = CreateDefaultSubobject<UAlsCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetMesh());
	Camera->SetRelativeRotation_Direct({0.0f, 90.0f, 0.0f});
}

void AAlsSimpleCharacterExample::NotifyControllerChanged()
{
	const auto* PreviousPlayer{Cast<APlayerController>(PreviousController)};
	if (IsValid(PreviousPlayer))
	{
		auto* InputSubsystem{ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PreviousPlayer->GetLocalPlayer())};
		if (IsValid(InputSubsystem))
		{
			InputSubsystem->RemoveMappingContext(InputMappingContext);
		}
	}

	auto* NewPlayer{Cast<APlayerController>(GetController())};
	if (IsValid(NewPlayer))
	{
		NewPlayer->InputYawScale_DEPRECATED = 1.0f;
		NewPlayer->InputPitchScale_DEPRECATED = 1.0f;
		NewPlayer->InputRollScale_DEPRECATED = 1.0f;

		auto* InputSubsystem{ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(NewPlayer->GetLocalPlayer())};
		if (IsValid(InputSubsystem))
		{
			InputSubsystem->AddMappingContext(InputMappingContext, 0);
		}
	}

	Super::NotifyControllerChanged();
}

void AAlsSimpleCharacterExample::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	if (Camera->IsActive())
	{
		Camera->GetViewInfo(ViewInfo);
		return;
	}

	Super::CalcCamera(DeltaTime, ViewInfo);
}

void AAlsSimpleCharacterExample::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);

	auto* EnhancedInput{Cast<UEnhancedInputComponent>(Input)};
	if (IsValid(EnhancedInput))
	{
		EnhancedInput->BindAction(LookMouseAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnLookMouse);
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnLook);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnMove);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnSprint);
		EnhancedInput->BindAction(WalkAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnWalk);
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnCrouch);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnJump);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnAim);
		EnhancedInput->BindAction(RagdollAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRagdoll);
		EnhancedInput->BindAction(RollAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRoll);
		EnhancedInput->BindAction(RotationModeAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnRotationMode);
		EnhancedInput->BindAction(ViewModeAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnViewMode);
		EnhancedInput->BindAction(SwitchShoulderAction, ETriggerEvent::Triggered, this, &ThisClass::Input_OnSwitchShoulder);
	}
}

void AAlsSimpleCharacterExample::Input_OnLookMouse(const FInputActionValue& ActionValue)
{
	const auto Value{ActionValue.Get<FVector2D>()};

	AddControllerPitchInput(Value.Y * LookUpMouseSensitivity);
	AddControllerYawInput(Value.X * LookRightMouseSensitivity);
}

void AAlsSimpleCharacterExample::Input_OnLook(const FInputActionValue& ActionValue)
{
	const auto Value{ActionValue.Get<FVector2D>()};

	const auto TimeDilation{GetWorldSettings()->GetEffectiveTimeDilation()};
	const auto DeltaTime{TimeDilation > UE_SMALL_NUMBER ? GetWorld()->GetDeltaSeconds() / TimeDilation : GetWorld()->DeltaRealTimeSeconds};

	AddControllerPitchInput(Value.Y * LookUpRate * DeltaTime);
	AddControllerYawInput(Value.X * LookRightRate * DeltaTime);
}

void AAlsSimpleCharacterExample::Input_OnMove(const FInputActionValue& ActionValue)
{
	const auto Value{UAlsMath::ClampMagnitude012D(ActionValue.Get<FVector2D>())};

	const auto ForwardDirection{UAlsMath::AngleToDirectionXY(UE_REAL_TO_FLOAT(AlsComponent->GetViewState().Rotation.Yaw))};
	const auto RightDirection{UAlsMath::PerpendicularCounterClockwiseXY(ForwardDirection)};

	AddMovementInput(ForwardDirection * Value.Y + RightDirection * Value.X);
}

void AAlsSimpleCharacterExample::Input_OnSprint(const FInputActionValue& ActionValue)
{
	AlsComponent->SetDesiredGait(ActionValue.Get<bool>() ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
}

void AAlsSimpleCharacterExample::Input_OnWalk()
{
	if (AlsComponent->GetDesiredGait() == AlsGaitTags::Walking)
	{
		AlsComponent->SetDesiredGait(AlsGaitTags::Running);
	}
	else if (AlsComponent->GetDesiredGait() == AlsGaitTags::Running)
	{
		AlsComponent->SetDesiredGait(AlsGaitTags::Walking);
	}
}

void AAlsSimpleCharacterExample::Input_OnCrouch()
{
	if (AlsComponent->GetDesiredStance() == AlsStanceTags::Standing)
	{
		AlsComponent->SetDesiredStance(AlsStanceTags::Crouching);
	}
	else if (AlsComponent->GetDesiredStance() == AlsStanceTags::Crouching)
	{
		AlsComponent->SetDesiredStance(AlsStanceTags::Standing);
	}
}

void AAlsSimpleCharacterExample::Input_OnJump(const FInputActionValue& ActionValue)
{
	if (ActionValue.Get<bool>())
	{
		if (AlsComponent->TryStopRagdolling())
		{
			return;
		}

		if (AlsComponent->TryStartMantlingGrounded())
		{
			return;
		}

		if (AlsComponent->GetStance() == AlsStanceTags::Crouching)
		{
			AlsComponent->SetDesiredStance(AlsStanceTags::Standing);
			return;
		}

		Jump();
	}
	else
	{
		StopJumping();
	}
}

void AAlsSimpleCharacterExample::Input_OnAim(const FInputActionValue& ActionValue)
{
	AlsComponent->SetDesiredAiming(ActionValue.Get<bool>());
}

void AAlsSimpleCharacterExample::Input_OnRagdoll()
{
	if (!AlsComponent->TryStopRagdolling())
	{
		AlsComponent->StartRagdolling();
	}
}

void AAlsSimpleCharacterExample::Input_OnRoll()
{
	static constexpr auto PlayRate{1.3f};

	AlsComponent->TryStartRolling(PlayRate);
}

void AAlsSimpleCharacterExample::Input_OnRotationMode()
{
	AlsComponent->SetDesiredRotationMode(AlsComponent->GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection
		                       ? AlsRotationModeTags::LookingDirection
		                       : AlsRotationModeTags::VelocityDirection);
}

void AAlsSimpleCharacterExample::Input_OnViewMode()
{
	AlsComponent->SetViewMode(AlsComponent->GetViewMode() == AlsViewModeTags::ThirdPerson ? AlsViewModeTags::FirstPerson : AlsViewModeTags::ThirdPerson);
}

// ReSharper disable once CppMemberFunctionMayBeConst
void AAlsSimpleCharacterExample::Input_OnSwitchShoulder()
{
	Camera->SetRightShoulder(!Camera->IsRightShoulder());
}

void AAlsSimpleCharacterExample::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& Unused, float& VerticalLocation)
{
	if (Camera->IsActive())
	{
		Camera->DisplayDebug(Canvas, DisplayInfo, VerticalLocation);
	}

	Super::DisplayDebug(Canvas, DisplayInfo, Unused, VerticalLocation);
}
