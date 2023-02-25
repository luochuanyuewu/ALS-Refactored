#include "ALSCameraModule.h"

#include "AlsCameraComponent.h"
#include "AlsComponent.h"
#include "GameFramework/HUD.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FALSCameraModule"

void FALSCameraModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (!IsRunningDedicatedServer())
	{
		AHUD::OnShowDebugInfo.AddStatic(&UAlsCameraComponent::OnShowDebugInfo);
	}
}

void FALSCameraModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FALSCameraModule, ALSCamera)

#undef LOCTEXT_NAMESPACE

