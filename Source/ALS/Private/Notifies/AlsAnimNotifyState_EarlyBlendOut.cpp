#include "Notifies/AlsAnimNotifyState_EarlyBlendOut.h"

#include "AlsComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsAnimNotifyState_EarlyBlendOut)

UAlsAnimNotifyState_EarlyBlendOut::UAlsAnimNotifyState_EarlyBlendOut()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif

	bIsNativeBranchingPoint = true;
}

FString UAlsAnimNotifyState_EarlyBlendOut::GetNotifyName_Implementation() const
{
	return FString{TEXTVIEW("Als Early Blend Out")};
}

void UAlsAnimNotifyState_EarlyBlendOut::NotifyTick(USkeletalMeshComponent *Mesh, UAnimSequenceBase *Animation,
												   const float DeltaTime, const FAnimNotifyEventReference &EventReference)
{
	Super::NotifyTick(Mesh, Animation, DeltaTime, EventReference);

	const auto *Montage{Cast<UAnimMontage>(Animation)};
	auto *AnimationInstance{IsValid(Montage) ? Mesh->GetAnimInstance() : nullptr};
	// const auto* Character{IsValid(AnimationInstance) ? Cast<AAlsCharacter>(Mesh->GetOwner()) : nullptr};
	const UAlsComponent *AlsComponent{IsValid(AnimationInstance) ? UAlsComponent::FindAlsComponent(Mesh->GetOwner()) : nullptr};

	if (IsValid(AlsComponent) &&
		((bCheckInput && AlsComponent->GetLocomotionState().bHasInput) ||
		 (bCheckLocomotionMode && AlsComponent->GetLocomotionMode() == LocomotionModeEquals) ||
		 (bCheckRotationMode && AlsComponent->GetRotationMode() == RotationModeEquals) ||
		 (bCheckStance && AlsComponent->GetStance() == StanceEquals)))
	{
		AnimationInstance->Montage_Stop(BlendOutDuration, Montage);
	}
}
