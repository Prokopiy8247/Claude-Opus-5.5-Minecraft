// Editor module: procedural asset generation (textures, materials, sounds, maps) and import pipelines.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOpus55Editor, Log, All);

class FUnreal_MinecraftEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
