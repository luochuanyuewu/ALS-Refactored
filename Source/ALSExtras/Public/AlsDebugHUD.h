// "// Copyright 2021 luochuanyuewu. All Rights Reserved."

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/HUD.h"
#include "AlsDebugHUD.generated.h"

UCLASS(ClassGroup=Als)
class ALSEXTRAS_API AAlsDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAlsDebugHUD();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void DisplayDebug(UCanvas* InCanvas, const FDebugDisplayInfo& InDebugDisplay, float& YL, float& YPos) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
