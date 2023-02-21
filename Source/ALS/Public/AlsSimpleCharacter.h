// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AlsSimpleCharacter.generated.h"

class UAlsComponent;

UCLASS()
class ALS_API AAlsSimpleCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AAlsSimpleCharacter(const FObjectInitializer& ObjectInitializer);

	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

	virtual bool CanCrouch() const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void OnRep_ReplicatedBasedMovement() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Restart() override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual FRotator GetViewRotation() const override;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Als Character")
	TObjectPtr<UAlsComponent> AlsComponent;



};
