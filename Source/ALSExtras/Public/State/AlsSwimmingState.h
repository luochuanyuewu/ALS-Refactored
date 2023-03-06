// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AlsSwimmingState.generated.h"

USTRUCT(BlueprintType)
struct ALSEXTRAS_API FAlsSwimmingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	bool bDiving{false};
};