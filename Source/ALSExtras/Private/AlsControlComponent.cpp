﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "AlsControlComponent.h"
#include "AlsCameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AlsComponent.h"
#include "GameFramework/Character.h"
#include "Utility/AlsMath.h"


// Sets default values for this component's properties
UAlsControlComponent::UAlsControlComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UAlsControlComponent::OnRegister()
{
	Super::OnRegister();
	if (APlayerController* PC =  Cast<APlayerController>(GetOwner()))
	{
		OwnerController = PC;
		PC->OnPossessedPawnChanged.AddDynamic(this,&ThisClass::UAlsControlComponent::OnPossessedPawnChanged);
	}
}

void UAlsControlComponent::OnUnregister()
{
	if (APlayerController* PC =  Cast<APlayerController>(GetOwner()))
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this,&ThisClass::UAlsControlComponent::OnPossessedPawnChanged);
	}
	Super::OnUnregister();
}



void UAlsControlComponent::OnPossessedPawnChanged_Implementation(APawn* OldPawn, APawn* NewPawn)
{
	if (bAlreadySetup)
		return;
	PossessedCharacter = Cast<ACharacter>(NewPawn);

	SetupInputs();

	bAlreadySetup = true;
}

void UAlsControlComponent::SetupInputComponent(UInputComponent* InputComponent)
{
	if (bAlreadyBoundInput)
		return;
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->ClearActionEventBindings();
		EnhancedInputComponent->ClearActionValueBindings();
		EnhancedInputComponent->ClearDebugKeyBindings();
		BindActions();
	}
	else
	{
		UE_LOG(LogTemp, Fatal, TEXT("ALS Community requires Enhanced Input System to be activated in project settings to function properly"));
	}

	bAlreadyBoundInput = true;
}

void UAlsControlComponent::OnCheckInputComponent()
{
	if (OwnerController!= nullptr && OwnerController->InputComponent != nullptr)
	{
		if (bAlreadySetup)
		{
			Timer_CheckInputComponent.Invalidate();
			SetupInputComponent(OwnerController->InputComponent);
		}
	}
}

void UAlsControlComponent::BindActions()
{
	if (OwnerController)
	{
		if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(OwnerController->InputComponent))
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
}

void UAlsControlComponent::BeginPlay()
{
	Super::BeginPlay();
	if (OwnerController)
	{
		if (OwnerController->GetPawn() != nullptr)
		{
			OnPossessedPawnChanged(nullptr,OwnerController->GetPawn());	
		}
	}
	if (!bAlreadyBoundInput)
	{
		GetWorld()->GetTimerManager().SetTimer(Timer_CheckInputComponent,FTimerDelegate::CreateUObject(this,&ThisClass::OnCheckInputComponent),0.2f,true);
	}
}

void UAlsControlComponent::SetupInputs()
{
	if (PossessedCharacter && OwnerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OwnerController->GetLocalPlayer()))
		{
			FModifyContextOptions Options;
			Options.bForceImmediately = 1;
			Subsystem->AddMappingContext(InputMappingContext, 1, Options);
		}
	}
}

void UAlsControlComponent::Input_OnLookMouse(const FInputActionValue& ActionValue)
{
	const auto Value{ActionValue.Get<FVector2D>()};

	PossessedCharacter->AddControllerPitchInput(Value.Y * LookUpMouseSensitivity);
	PossessedCharacter->AddControllerYawInput(Value.X * LookRightMouseSensitivity);
}

void UAlsControlComponent::Input_OnLook(const FInputActionValue& ActionValue)
{
	const auto Value{ActionValue.Get<FVector2D>()};

	const auto TimeDilation{PossessedCharacter->GetWorldSettings()->GetEffectiveTimeDilation()};
	const auto DeltaTime{TimeDilation > UE_SMALL_NUMBER ? GetWorld()->GetDeltaSeconds() / TimeDilation : GetWorld()->DeltaRealTimeSeconds};

	PossessedCharacter->AddControllerPitchInput(Value.Y * LookUpRate * DeltaTime);
	PossessedCharacter->AddControllerYawInput(Value.X * LookRightRate * DeltaTime);
}

void UAlsControlComponent::Input_OnMove(const FInputActionValue& ActionValue)
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		const auto Value{UAlsMath::ClampMagnitude012D(ActionValue.Get<FVector2D>())};

		const auto ForwardDirection{UAlsMath::AngleToDirectionXY(UE_REAL_TO_FLOAT(AlsComponent->GetViewState().Rotation.Yaw))};
		const auto RightDirection{UAlsMath::PerpendicularCounterClockwiseXY(ForwardDirection)};

		PossessedCharacter->AddMovementInput(ForwardDirection * Value.Y + RightDirection * Value.X);
	}
}

void UAlsControlComponent::Input_OnSprint(const FInputActionValue& ActionValue)
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		AlsComponent->SetDesiredGait(ActionValue.Get<bool>() ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
	}
}

void UAlsControlComponent::Input_OnWalk()
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
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
}

void UAlsControlComponent::Input_OnCrouch()
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
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

}

void UAlsControlComponent::Input_OnJump(const FInputActionValue& ActionValue)
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
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

			PossessedCharacter->Jump();
		}
		else
		{
			PossessedCharacter->StopJumping();
		}
	}

}

void UAlsControlComponent::Input_OnAim(const FInputActionValue& ActionValue)
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		AlsComponent->SetDesiredAiming(ActionValue.Get<bool>());
	}
}

void UAlsControlComponent::Input_OnRagdoll()
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		if (!AlsComponent->TryStopRagdolling())
		{
			AlsComponent->StartRagdolling();
		}
	}
}

void UAlsControlComponent::Input_OnRoll()
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		static constexpr auto PlayRate{1.3f};

		AlsComponent->TryStartRolling(PlayRate);
	}

}

void UAlsControlComponent::Input_OnRotationMode()
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		AlsComponent->SetDesiredRotationMode(AlsComponent->GetDesiredRotationMode() == AlsRotationModeTags::VelocityDirection
						   ? AlsRotationModeTags::LookingDirection
						   : AlsRotationModeTags::VelocityDirection);
	}

}

void UAlsControlComponent::Input_OnViewMode()
{
	if (UAlsComponent* AlsComponent = UAlsComponent::FindAlsComponent(PossessedCharacter))
	{
		AlsComponent->SetViewMode(AlsComponent->GetViewMode() == AlsViewModeTags::ThirdPerson ? AlsViewModeTags::FirstPerson : AlsViewModeTags::ThirdPerson);
	}
}

// ReSharper disable once CppMemberFunctionMayBeConst
void UAlsControlComponent::Input_OnSwitchShoulder()
{
	if (UAlsCameraComponent* Camera = UAlsCameraComponent::FindAlsCameraComponent(PossessedCharacter))
	{
		Camera->SetRightShoulder(!Camera->IsRightShoulder());
	}
}