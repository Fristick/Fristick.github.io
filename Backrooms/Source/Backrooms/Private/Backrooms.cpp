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
		// v4.15 : OnPostEngineInit est deprecie en 5.8 (GetOnPostEngineInit)
		FCoreDelegates::GetOnPostEngineInit().AddLambda([]() { BRLoc::ApplyStartupLanguage(); });
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FBackroomsModule, Backrooms, "Backrooms");
