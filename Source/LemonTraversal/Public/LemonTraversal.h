// Copyright Cieran Nicholas. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Log category for the LemonTraversal plugin. */
DECLARE_LOG_CATEGORY_EXTERN(LogLemonTraversal, Log, All);

class FLemonTraversalModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
