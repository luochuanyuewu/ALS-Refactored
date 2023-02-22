// Fill out your copyright notice in the Description page of Project Settings.


#include "AlsSimpleCharacter.h"

#include "AlsComponent.h"


// Sets default values
AAlsSimpleCharacter::AAlsSimpleCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AlsComponent = CreateDefaultSubobject<UAlsComponent>(TEXT("AlsComponent"));
}

UAlsComponent* AAlsSimpleCharacter::GetAlsComponent_Implementation() const
{
	return AlsComponent;
}

// Called when the game starts or when spawned
void AAlsSimpleCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AAlsSimpleCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if (UAlsComponent* Als = GetAlsComponent())
	{
		Als->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
	}
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
}

bool AAlsSimpleCharacter::CanCrouch() const
{
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

