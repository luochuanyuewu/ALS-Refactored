#include "AlsCameraAnimationInstance.h"

#include "AlsCameraComponent.h"
#include "AlsCharacter.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCameraAnimationInstance)

void UAlsCameraAnimationInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Character = Cast<ACharacter>(GetOwningActor());
	AlsComponent = UAlsComponent::FindAlsComponent(GetOwningActor());
	Camera = Cast<UAlsCameraComponent>(GetSkelMeshComponent());

#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		// Use default objects for editor preview.

		// if (!IsValid(Character))
		// {
		// 	Character = GetMutableDefault<ACharacter>();
		// }

		if (!IsValid(AlsComponent))
		{
			AlsComponent = GetMutableDefault<UAlsComponent>();
		}

		if (!IsValid(Camera))
		{
			Camera = GetMutableDefault<UAlsCameraComponent>();
		}
	}
#endif
}

void UAlsCameraAnimationInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();

	if (!IsValid(AlsComponent))
	{
		AlsComponent = UAlsComponent::FindAlsComponent(GetOwningActor());
	}
}

void UAlsCameraAnimationInstance::NativeUpdateAnimation(const float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if (!IsValid(Character) || !IsValid(AlsComponent) || !IsValid(Camera))
	{
		return;
	}

	ViewMode = AlsComponent->GetViewMode();
	LocomotionMode = AlsComponent->GetLocomotionMode();
	RotationMode = AlsComponent->GetRotationMode();
	Stance = AlsComponent->GetStance();
	Gait = AlsComponent->GetGait();
	LocomotionAction = AlsComponent->GetLocomotionAction();

	bRightShoulder = Camera->IsRightShoulder();
}
