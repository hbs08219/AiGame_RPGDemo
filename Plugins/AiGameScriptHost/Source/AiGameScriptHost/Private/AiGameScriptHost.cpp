// AiGameScriptHost runtime module entry point.
// UPuertsHost lives in Puerts/PuertsHost.cpp; this file supplies the UE module factory
// required by the AiGameScriptHost.uplugin Modules declaration.

#include "Modules/ModuleManager.h"

class FAiGameScriptHostModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FAiGameScriptHostModule, AiGameScriptHost)
