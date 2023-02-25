// "// Copyright 2021 luochuanyuewu. All Rights Reserved."


#include "AlsDebugHUD.h"

#include "AlsCameraComponent.h"
#include "AlsComponent.h"


// Sets default values
AAlsDebugHUD::AAlsDebugHUD()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AAlsDebugHUD::BeginPlay()
{
	Super::BeginPlay();
}

void AAlsDebugHUD::DisplayDebug(UCanvas* InCanvas, const FDebugDisplayInfo& InDebugDisplay, float& YL, float& YPos)
{
	if (UAlsComponent* Als = UAlsComponent::FindAlsComponent(GetOwningPawn()))
	{
		Als->DisplayDebug(InCanvas, InDebugDisplay, YL, YPos);
	}
	if (UAlsCameraComponent* AlsCamera = UAlsCameraComponent::FindAlsCameraComponent(GetOwningPawn()))
	{
		if (AlsCamera->IsActive())
		{
			AlsCamera->DisplayDebug(InCanvas, InDebugDisplay, YPos);
		}
	}
	Super::DisplayDebug(InCanvas, InDebugDisplay, YL, YPos);
}

// Called every frame
void AAlsDebugHUD::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

