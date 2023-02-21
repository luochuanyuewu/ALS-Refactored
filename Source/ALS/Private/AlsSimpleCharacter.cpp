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

// Called when the game starts or when spawned
void AAlsSimpleCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AAlsSimpleCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	AlsComponent->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
}

bool AAlsSimpleCharacter::CanCrouch() const
{
	return Super::CanCrouch();
}

void AAlsSimpleCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	AlsComponent->OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AAlsSimpleCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	AlsComponent->OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void AAlsSimpleCharacter::PostNetReceiveLocationAndRotation()
{
	AlsComponent->PostNetReceiveLocationAndRotation();
	Super::PostNetReceiveLocationAndRotation();
}

void AAlsSimpleCharacter::OnRep_ReplicatedBasedMovement()
{
	AlsComponent->OnRep_ReplicatedBasedMovement();
	Super::OnRep_ReplicatedBasedMovement();
}

void AAlsSimpleCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	AlsComponent->PossessedBy(NewController);
}

void AAlsSimpleCharacter::Restart()
{
	Super::Restart();
	AlsComponent->Restart();
}

bool AAlsSimpleCharacter::CanJumpInternal_Implementation() const
{
	if (AlsComponent->CanJump())
	{
		return true;
	}
	return Super::CanJumpInternal_Implementation();
}

void AAlsSimpleCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();
	AlsComponent->OnJumped();
}

FRotator AAlsSimpleCharacter::GetViewRotation() const
{
	// return Super::GetViewRotation();
	return AlsComponent->GetViewRotation();
}

// Called every frame
void AAlsSimpleCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
