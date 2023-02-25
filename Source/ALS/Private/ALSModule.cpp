#include "ALSModule.h"

#include "AlsComponent.h"
#include "GameFramework/HUD.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FALSModule"

void FALSModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	if (!IsRunningDedicatedServer())
	{
		AHUD::OnShowDebugInfo.AddStatic(&UAlsComponent::OnShowDebugInfo);
	}
}

void FALSModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

IMPLEMENT_MODULE(FALSModule, ALS)

#undef LOCTEXT_NAMESPACE

