// "// Copyright 2021 luochuanyuewu. All Rights Reserved."


#include "AlsPlayerCameraManager.h"

#include "AlsCameraComponent.h"

void AAlsPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	if (OutVT.Target)
	{
		FVector OutLocation;
		FRotator OutRotation;
		float OutFOV;

		if (BlueprintUpdateCamera(OutVT.Target, OutLocation, OutRotation, OutFOV))
		{
			OutVT.POV.Location = OutLocation;
			OutVT.POV.Rotation = OutRotation;
			OutVT.POV.FOV = OutFOV;
		}
		else if (CustomUpdateCamera(OutVT))
		{
			
		}
		else
		{
			OutVT.Target->CalcCamera(DeltaTime, OutVT.POV);
		}
	}
}

bool AAlsPlayerCameraManager::CustomUpdateCamera(FTViewTarget& OutVT)
{
	if (GetOwningPlayerController()->GetPawn())
	{
		if (UAlsCameraComponent* AlsCamera = UAlsCameraComponent::FindAlsCameraComponent(GetOwningPlayerController()->GetPawn()))
		{
			if (AlsCamera->IsActive())
			{
				AlsCamera->GetViewInfo(OutVT.POV);
				return true;
			}
		}
	}
	return false;
}
