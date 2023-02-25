// "// Copyright 2021 luochuanyuewu. All Rights Reserved."

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "AlsPlayerCameraManager.generated.h"

/**
 * 
 */
UCLASS(ClassGroup=Als)
class ALSCAMERA_API AAlsPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

protected:

	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;

	bool CustomUpdateCamera(FTViewTarget& OutVT);
};
