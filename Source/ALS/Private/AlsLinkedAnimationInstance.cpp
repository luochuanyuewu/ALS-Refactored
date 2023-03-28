#include "AlsLinkedAnimationInstance.h"

#include "AlsAnimationInstance.h"
#include "AlsComponent.h"
#include "GameFramework/Character.h"
#include "Utility/AlsMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsLinkedAnimationInstance)

UAlsLinkedAnimationInstance::UAlsLinkedAnimationInstance()
{
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
	bUseMainInstanceMontageEvaluationData = true;
}

void UAlsLinkedAnimationInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Parent = Cast<UAlsAnimationInstance>(GetSkelMeshComponent()->GetAnimInstance());
	Character = Cast<ACharacter>(GetOwningActor());
	AlsComponent = UAlsComponent::FindAlsComponent(GetOwningActor());

#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		// Use default objects for editor preview.

		if (!Parent.IsValid())
		{
			Parent = GetMutableDefault<UAlsAnimationInstance>();
		}

		if (!IsValid(Character))
		{
			Character = GetMutableDefault<ACharacter>();
		}
		if (!IsValid(AlsComponent))
		{
			AlsComponent = GetMutableDefault<UAlsComponent>();
		}
	}
#endif
}

void UAlsLinkedAnimationInstance::NativeBeginPlay()
{
	ALS_ENSURE_MESSAGE(Parent.IsValid(),
	                   TEXT("%s (%s) should only be used as a linked animation instance within the %s animation blueprint!"),
	                   ALS_GET_TYPE_STRING(UAlsLinkedAnimationInstance).GetData(), *GetClass()->GetName(),
	                   ALS_GET_TYPE_STRING(UAlsAnimationInstance).GetData());

	Super::NativeBeginPlay();
}
