#include "Modules/ModuleManager.h"

#include "VPBroadcastSubsystem.h"

class FVPBroadcastModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FVPBroadcastModule, VPBroadcast)