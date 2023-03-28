#include "Notifies/AlsAnimNotifyState_SetLocomotionAction.h"

#include "AlsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Utility/AlsUtility.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsAnimNotifyState_SetLocomotionAction)

UAlsAnimNotifyState_SetLocomotionAction::UAlsAnimNotifyState_SetLocomotionAction()
{
	bIsNativeBranchingPoint = true;
}

FString UAlsAnimNotifyState_SetLocomotionAction::GetNotifyName_Implementation() const
{
	TStringBuilder<256> NotifyNameBuilder;

	NotifyNameBuilder << TEXTVIEW("Als Set Locomotion Action: ")
		<< FName::NameToDisplayString(UAlsUtility::GetSimpleTagName(LocomotionAction).ToString(), false);

	return FString{NotifyNameBuilder};
}

void UAlsAnimNotifyState_SetLocomotionAction::NotifyBegin(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
                                                          const float Duration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(Mesh, Animation, Duration, EventReference);

	UAlsComponent* AlsComponent{UAlsComponent::FindAlsComponent(Mesh->GetOwner())};

	if (IsValid(AlsComponent))
	{
		AlsComponent->SetLocomotionAction(LocomotionAction);
	}
}

void UAlsAnimNotifyState_SetLocomotionAction::NotifyEnd(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation,
                                                        const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(Mesh, Animation, EventReference);

	UAlsComponent* AlsComponent{UAlsComponent::FindAlsComponent(Mesh->GetOwner())};

	if (IsValid(AlsComponent) && AlsComponent->GetLocomotionAction() == LocomotionAction)
	{
		AlsComponent->SetLocomotionAction(FGameplayTag::EmptyTag);
	}
}
