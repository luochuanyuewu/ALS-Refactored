// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "AlsSwimmingComponent.generated.h"

class ACharacter;
class UAlsComponent;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ALSEXTRAS_API UAlsSwimmingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAlsSwimmingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	// Locomotion Mode
protected:

	UFUNCTION()
	virtual void OnLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode);

	virtual void BeginSwimming();

	virtual void TickSwimming(float DeltaTime);
	
	virtual void EndSwimming();
	
	
private:

	UFUNCTION()
	virtual void OnMovementModeChanged(ACharacter* InCharacter, EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0);

	bool bSwimming = false;
	
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter = nullptr;

	UPROPERTY()
	TObjectPtr<UAlsComponent> AlsComponent = nullptr;

	UPROPERTY()
	FVector2D DefaultCapsuleSize;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	float DefaultBuoyancy = 2.0f;
	
};
