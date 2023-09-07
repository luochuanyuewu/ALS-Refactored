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
	AAlsSimpleCharacter(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Als Character")
	UAlsComponent* GetAlsComponent() const;
	virtual UAlsComponent* GetAlsComponent_Implementation() const;


#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif


	virtual void PreRegisterAllComponents() override;

	virtual void PostInitializeComponents() override;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	virtual bool CanCrouch() const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void OnRep_ReplicatedBasedMovement() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Restart() override;
	// virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual FRotator GetViewRotation() const override;
	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime) override final;
};
