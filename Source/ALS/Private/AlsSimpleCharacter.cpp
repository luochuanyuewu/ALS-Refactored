// Fill out your copyright notice in the Description page of Project Settings.


#include "AlsSimpleCharacter.h"

#include "AlsComponent.h"
#include "Components/CapsuleComponent.h"


// Sets default values
AAlsSimpleCharacter::AAlsSimpleCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	bClientCheckEncroachmentOnNetUpdate = true; // Required for bSimGravityDisabled to be updated.

	GetCapsuleComponent()->InitCapsuleSize(30.0f, 90.0f);

	GetMesh()->SetRelativeLocation_Direct({0.0f, 0.0f, -92.0f});
	GetMesh()->SetRelativeRotation_Direct({0.0f, -90.0f, 0.0f});

	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

	GetMesh()->bEnableUpdateRateOptimizations = false;

	GetMesh()->bUpdateJointsFromAnimation = true; // Required for the flail animation to work properly when ragdolling.

	// This will prevent the editor from combining component details with actor details.
	// Component details can still be accessed from the actor's component hierarchy.

	#if WITH_EDITOR
	StaticClass()->FindPropertyByName(TEXT("Mesh"))->SetPropertyFlags(CPF_DisableEditOnInstance);
	StaticClass()->FindPropertyByName(TEXT("CapsuleComponent"))->SetPropertyFlags(CPF_DisableEditOnInstance);
	StaticClass()->FindPropertyByName(TEXT("CharacterMovement"))->SetPropertyFlags(CPF_DisableEditOnInstance);
#endif
}

#if WITH_EDITOR
bool AAlsSimpleCharacter::CanEditChange(const FProperty* Property) const
{
	return Super::CanEditChange(Property) &&
		   Property->GetFName() != GET_MEMBER_NAME_CHECKED(ThisClass, bUseControllerRotationPitch) &&
		   Property->GetFName() != GET_MEMBER_NAME_CHECKED(ThisClass, bUseControllerRotationYaw) &&
		   Property->GetFName() != GET_MEMBER_NAME_CHECKED(ThisClass, bUseControllerRotationRoll);
}

void AAlsSimpleCharacter::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();
}

void AAlsSimpleCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}
#endif


UAlsComponent* AAlsSimpleCharacter::GetAlsComponent_Implementation() const
{
	return nullptr;
}

// Called when the game starts or when spawned
void AAlsSimpleCharacter::BeginPlay()
{
	Super::BeginPlay();
}


bool AAlsSimpleCharacter::CanCrouch() const
{
	 //This allows to execute the ACharacter::Crouch() function properly when bIsCrouched is true.
	 if (UAlsComponent* Als = GetAlsComponent())
	 {
	 	return bIsCrouched || Super::CanCrouch();
	 }
	return Super::CanCrouch();
}

void AAlsSimpleCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	}
}

void AAlsSimpleCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	}
}

void AAlsSimpleCharacter::PostNetReceiveLocationAndRotation()
{
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->PostNetReceiveLocationAndRotation();
	}
	Super::PostNetReceiveLocationAndRotation();
}

void AAlsSimpleCharacter::OnRep_ReplicatedBasedMovement()
{
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->OnRep_ReplicatedBasedMovement();
	}
	Super::OnRep_ReplicatedBasedMovement();
}

void AAlsSimpleCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->PossessedBy(NewController);
	}
}

void AAlsSimpleCharacter::Restart()
{
	Super::Restart();
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->Restart();
	}
}

bool AAlsSimpleCharacter::CanJumpInternal_Implementation() const
{

	if (UAlsComponent* Als = GetAlsComponent())
	{
		if (Als->CanJump())
		{
			return true;
		}
	}
	return Super::CanJumpInternal_Implementation();
}

void AAlsSimpleCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->OnJumped();
	}
}

FRotator AAlsSimpleCharacter::GetViewRotation() const
{
	if (UAlsComponent* Als = GetAlsComponent())
	{
		return Als->GetViewRotation();
	}
	return Super::GetViewRotation();
}

void AAlsSimpleCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	UAlsComponent* Als = GetAlsComponent();
	if (Als == nullptr)
	{
		Super::FaceRotation(NewControlRotation, DeltaTime);
	}
}

