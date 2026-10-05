#include "Backrooms.h"
#include "BRLoc.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogBackrooms);

// v4.8 : la langue du joueur est appliquee des que le moteur est pret, avant le menu
class FBackroomsModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FCoreDelegates::OnPostEngineInit.AddLambda([]() { BRLoc::ApplyStartupLanguage(); });
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FBackroomsModule, Backrooms, "Backrooms");
