// Fill out your copyright notice in the Description page of Project Settings.


#include "SwimmingSystem/AlsSwimmingComponent.h"

#include "AlsComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utility/AlsExtraGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsSwimmingComponent)

// Sets default values for this component's properties
UAlsSwimmingComponent::UAlsSwimmingComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UAlsSwimmingComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (OwnerCharacter == nullptr)
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (!IsComponentTickEnabled())
	{
		SetComponentTickEnabled(true);
	}

	DefaultCapsuleSize.X = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	DefaultCapsuleSize.Y = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	OwnerCharacter->GetCharacterMovement()->Buoyancy = DefaultBuoyancy;
	AlsComponent = UAlsComponent::FindAlsComponent(OwnerCharacter);

	OwnerCharacter->MovementModeChangedDelegate.AddDynamic(this, &ThisClass::OnMovementModeChanged);
	AlsComponent->OnLocomotionModeChangedEvent.AddDynamic(this, &ThisClass::OnLocomotionModeChanged);
}

void UAlsSwimmingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwnerCharacter)
	{
		OwnerCharacter->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::OnMovementModeChanged);
	}
	if (AlsComponent)
	{
		AlsComponent->OnLocomotionModeChangedEvent.RemoveDynamic(this, &ThisClass::OnLocomotionModeChanged);
	}
	Super::EndPlay(EndPlayReason);
}


// Called every frame
void UAlsSwimmingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (OwnerCharacter == nullptr)
	{
		return;
	}
	if (!OwnerCharacter->GetCharacterMovement()->IsSwimming() || !bSwimming)
	{
		return;
	}

	TickSwimming(DeltaTime);

	// ...
}

void UAlsSwimmingComponent::OnLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode)
{
	if (PreviousLocomotionMode != AlsLocomotionModeTags::Swimming && AlsComponent->GetLocomotionMode() == AlsLocomotionModeTags::Swimming)
	{
		BeginSwimming();
	}
	if (PreviousLocomotionMode == AlsLocomotionModeTags::Swimming && AlsComponent->GetLocomotionMode() != AlsLocomotionModeTags::Swimming)
	{
		EndSwimming();
	}
}

void UAlsSwimmingComponent::BeginSwimming()
{
	bSwimming = true;
}

void UAlsSwimmingComponent::TickSwimming(float DeltaTime)
{
}

void UAlsSwimmingComponent::EndSwimming()
{
	bSwimming = false;
}

void UAlsSwimmingComponent::OnMovementModeChanged(ACharacter* InCharacter, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	// Use the character movement mode to set the locomotion mode to the right value. This allows you to have a
	// custom set of movement modes but still use the functionality of the default character movement component.

	switch (OwnerCharacter->GetCharacterMovement()->MovementMode)
	{
	case MOVE_Swimming:
		AlsComponent->SetLocomotionMode(AlsLocomotionModeTags::Swimming);
		break;
	default:
		// SetLocomotionMode(FGameplayTag::EmptyTag);
		break;
	}
}
