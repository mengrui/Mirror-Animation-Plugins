// Copyright Terry Meng 2022 All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMirrorAnimationModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
