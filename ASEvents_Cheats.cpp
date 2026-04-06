//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ASEvents.h"
#include "ASEvents_Options.h"
#include "ASEvents_Cheats.h"
#include "ASEvents_Map.h"

#include <gameswf/ui/flashfx.h>
#include <gameswf/swf/character_handle.h>
#include "gameswf/as_classes/as_array.h"
#include <gameswf/gameswf.h>

#include "Menus/Hud/Hud.h"
#include "Menus/Menu/MenuManager.h"

#include "Game/Level/Level.h"

#include "Game/Shop/PlayerProfile.h"
#include "Game/Shop/NotificationsManager.h"
#include "Game/Shop/ShopManager.h"
#include "Game/Shop/ShopNames.h"

#include "ASEvents_Daily.h"

#include "Game/Utils/Utils.h"

#include "ApplicationInfo.h"
#include "Application.h"
#include "DeviceOptions.h"
#include "GameSettings.h"

#include "Gameplay/Core/AI/AIController.h"
#include "Gameplay/Core/Components/DebuffComponent.h"
#include "Gameplay/Core/Components/MissionComponent.h"

#include "GameStates/Menu/IGM/GS_InGameMenu.h"

#include "Glitch/PostEffects/PostEffects.h"

#include "Generic/Tracking/GlotManager.h"

#include "OnlineManagers/AHGSLobbyManager.h"
#include "OnlineManagers/SocialLibManager.h"
//#include "OnlineManagers/GLAdsManager.h"
#include "OnlineManagers/GameAdsManager.h"
#include "OnlineManagers/Clans/ClansManager.h"
#include "OnlineManagers/DlcManager.h"
#include "OnlineManagers/OnlineManager.h"
#include "OnlineManagers/AHCheatCommands.h"
#include "OnlineManagers/EventMessageCommand.h"
#include "OnlineManagers/SemEventManager.h"

#include "Multiplayer/MultiplayerCredits.h"
#include "Missions/MissionDataManager.h"
#include "Missions/EventsDataManager.h"
#include "Tutorials/TutorialManager.h"
#include "FirebaseManagers/DynamicLinks.h"

#include "IO/Audio/VoxSoundManager.h"
#include "MultiplayerManager/GameMpManager.h"
#include "GameServerMessages.h"
#if defined(OS_WD)
#include "steam/steam_api.h"
#include "steam/steamclientpublic.h"
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool k_markAllBRTargets;
extern bool s_forceChinaGacha;
extern bool isCheatShardQuestCompleteEnable;
extern float g_subscriptionDay;
extern float g_subscriptionMonth;
extern float g_newShopOverride;
extern float isNewDailyReward;
extern float fCheatDailyV2UnlockTime;
extern float g_undefeatedStreak;
static float s_cmpUSConsentExpiryTime;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const static int DEBUG_DATA_DEBUG = 1 << 0;
const static int DEBUG_DATA_CHEATS = 1 << 1;
const static int DEBUG_DATA_INFO = 1 << 2;
const static int DEBUG_DATA_SOUNDS = 1 << 3;
const static int DEBUG_DATA_CHEATS_ENDLEVEL = 1 << 6;
const static int DEBUG_DATA_CHEATS_BR = 1 << 7;
const static int DEBUG_DATA_ALL = DEBUG_DATA_DEBUG | DEBUG_DATA_CHEATS | DEBUG_DATA_INFO | DEBUG_DATA_SOUNDS | DEBUG_DATA_CHEATS_ENDLEVEL | DEBUG_DATA_CHEATS_BR;

#if defined(DISABLE_ALL_CHEATS)
const static int g_debugData = 0;
#elif PRESS_RELEASE_DEMO || E3_2014_DEMO_VERSION
const static int g_debugData = 0;
#else
const static int g_debugData = DEBUG_DATA_ALL;
#endif // PRESS_RELEASE_DEMO | E3_2014_DEMO_VERSION

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DebugControl
{
	enum Type
	{
		None,
		Button,
		Switch,
		Label,
		IntScroll,
		FloatScroll,
	};

	DebugControl(std::function<void(void)>&& onclick) : type(Button), onclick(std::move(onclick)) {}
	DebugControl(bool& var, std::function<void(void)>&& onclick = {}) : type(Switch), witch({ &var }), onclick(std::move(onclick)) {}
	DebugControl(float& fvalue, int min, int max, std::function<void(void)>&& onclick) : type(IntScroll), fscroll({ &fvalue }), min(min), max(max), onclick(std::move(onclick)) {}
	DebugControl(float& fvalue) : type(FloatScroll), fscroll({ &fvalue }) {}
	DebugControl(const char* value, std::function<std::string()>&& label = {}) : type(Label), label({ value }), genlabel(std::move(label)) {}

	Type type = None;

	std::function<void(void)> onclick;
	std::function<std::string()> genlabel;
	union
	{
		struct { const char* value; } label;
		struct { bool* value; } witch;
		struct { float* value; } fscroll;
		int64_t init = 0;
	};
	int32_t min = -1;
	int32_t max = -1;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CheatEntry
{
	k_asevent_option op_id;
	const char* label;
	DebugControl control;

	bool operator ==(k_asevent_option id) const { return op_id == id; }

	gameswf::ASObject* create(gameswf::Player* swfPlayer) const
	{
		auto swf = swfnew gameswf::ASObject(swfPlayer);
		swf->setMember("label", label);
		swf->setMember("id", op_id);
		switch (control.type)
		{
		case DebugControl::Button:
			swf->setMember("type", "button");
			swf->setMember("value", false);
			break;
		case DebugControl::Switch:
			swf->setMember("type", "switch");
			swf->setMember("value", *control.witch.value);
			break;
		case DebugControl::Label:
			swf->setMember("type", "text");
			swf->setMember("text",
				control.genlabel ? control.genlabel().c_str() :
				control.label.value ? control.label.value :
				"");
			break;
		case DebugControl::IntScroll:
			swf->setMember("type", "scrollBar");
			swf->setMember("value", *control.fscroll.value);
			swf->setMember("measureUnit", "#");
			if (control.min != 0 || control.max > 0)
			{
				swf->setMember("minimum", control.min);
				swf->setMember("maximum", control.max);
			}
			break;
		case DebugControl::FloatScroll:
			swf->setMember("type", "scrollBar");
			swf->setMember("value", *control.fscroll.value);
			break;
		case DebugControl::None:
		default:
			GLF_ASSERTMSG(false, "Unknown debug control type");
			break;
		}
		return swf;
	}

	void handle(const gameswf::ASValue& optionValue) const
	{
		switch (control.type)
		{
		case DebugControl::Button:
			if (optionValue.toInt() == 1)
				control.onclick();
			return;
		case DebugControl::Switch:
			*control.witch.value = optionValue.toBool();
			if (control.onclick)
				control.onclick();
			return;
		case DebugControl::IntScroll:
			*control.fscroll.value = optionValue.toFloat();
			if (control.onclick)
				control.onclick();
			return;
		case DebugControl::FloatScroll:
			*control.fscroll.value = optionValue.toFloat();
			return;
		case DebugControl::Label:
		case DebugControl::None:
		default:
			GLF_ASSERTMSG(false, "Unknown debug control type");
			return;
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CheatCategory
{
	const char* id;
	const char* label;
	int flags;
	std::vector<CheatEntry> entries;
	std::vector<CheatCategory> subcategories;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(DISABLE_ALL_CHEATS)
bool bShortSquadOps = false;
#endif // !defined(DISABLE_ALL_CHEATS)
void Cheats_ResetShortSquadOps()
{
#if !defined(DISABLE_ALL_CHEATS)
	bShortSquadOps = false;
#endif // !defined(DISABLE_ALL_CHEATS)
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<CheatCategory>& CheatCategories()
{
#if defined(DISABLE_ALL_CHEATS)
	static std::vector<CheatCategory> categories;
#else // defined(DISABLE_ALL_CHEATS)
	static bool bExistsQAGCFile = false;
	bExistsQAGCFile = DLC()->ExistsQAGCFile();

	static std::vector<CheatCategory> categories = 
	{
		{ "debug", "DEBUG", DEBUG_DATA_DEBUG, 
	{
#ifdef STORE_HESTIA_TAGS_INFO
		{ ASEO_showHestiaTags,		"Show Hestia Tags",				{ []() {
			const CShopManager::ShopString& hestiaTags = GetShopManager()->GetConfigTagsInfoCheats();
			showScrollableNotificationPopup(hestiaTags.c_str(), "showHestiaTags", "Hestia Config Tags");
		} } },
#endif // STORE_HESTIA_TAGS_INFO
		{ ASEO_saveProfileRefresh,	"SaveProfile&RefreshHestia",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SaveAndRefresh);
		} } },
		{ ASEO_none,				"About",						{ nullptr, []() {
			//text area: version & rev, server rev, data center, country, ads provider
			char AppVersion[8] = "";
			ApplicationInfo::GetVersion(AppVersion, 8);

			char SvnVersion[128] = "unknown";
			ApplicationInfo::GetSvnRevision(SvnVersion, 128);

			char ServerVersion[32] = "";
			ApplicationInfo::GetVersionForAbout(ServerVersion, 32);

			std::string adsProvider = ADS()->GetAdProvider();
			std::string deviceCountry = GetApplication()->GetCountryCode();

			char Version[1024];
			sprintf(Version, "Version: %s%s\nRevision: %s\nServer Revision: %s\nDatacenter: %s\nAds provider: %s\nCountry code: %s",
				AppVersion,
				DLC()->IsDlcEnabled() ? "_DLC" : "",//Version
				SvnVersion,//Revision
				ServerVersion,//Server Revision
				GetGaia()->GetDataCenter().c_str(),//Datacenter
				adsProvider.c_str(),//Ads provider
				deviceCountry.c_str()//Country code
			);
#ifdef DEBUG_DYNAMIC_LINKS
#if USE_DYNAMIC_LINKS
			if (Application::GetInstance()->m_dynamicLinksManager.get_raw_ptr())
			{
				auto dbgDynamicLinks = Application::GetDynamicLinkReceived();
				if (dbgDynamicLinks)
				{
					std::string tmpVer = Version;
					sprintf(Version, "%s\nDynamic Links:  received = %s;  URL = %s", tmpVer.c_str(),
							dbgDynamicLinks->m_received ? "yes" : "no", dbgDynamicLinks->m_link.c_str());
				}
			}

#endif // USE_DYNAMIC_LINKS
#endif // DEBUG_DYNAM
			return std::string(Version);
		} } },
		{ ASEO_none,				"Empty", ""},
		{ ASEO_none,				"ChooseTxt", "Choose one of \'+\' option for Reset Profile" },
		{ ASEO_none,				"Empty", "" },
		{ ASEO_RP_SkipMenuTutorialNoDrone,				"+ Skip Menu Tutorial (-droneOps)",				CLevel::sSkipMenuTutorialNoDrone },
		{ ASEO_RP_SkipInGameTutorial,					"+ Skip InGame Tutorial",						CLevel::sSkipInGameTutorial },
		{ ASEO_RP_SkipTutorialsNoArena,					"+ Skip Tutorials (-arena)",					CLevel::sSkipTutorialsNoArena },
		{ ASEO_RP_SetPlayer_BaseLevelMax,				"+ Set Player&Base Level Max",					CLevel::sSetPlayer_BaseLevelMax },
		{ ASEO_RP_UnlockAllLevels,						"+ Unlock All Levels",							CLevel::sUnlockAllLevels },
		{ ASEO_RP_FinishAllTutorials,					"+ Finish All Tutorials",						CLevel::sFinishAllTutorials },
		{ ASEO_resetProfile,		"Reset Profile",				{ []() {
#if defined(OS_WD) && defined(USE_STEAM_LOGIN)
			SteamUserStats()->ResetAllStats(true);
#endif
			if (!Application::GetInstance()->m_onlineActive)
			{
				PLAYER_PROFILE()->Reset();
				NOTIFICATIONS()->Reset();
			}
			else
			{
				GetOnlineManager()->GetLinkingManager()->LogoutAllSNS();
				GetOnlineManager()->ClearCachedData();
				GetOnlineManager()->ResetGDPRFlow();
				GetOnlineManager()->ResetAgeInGDPRFlow(true);
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetProfile);
				PLAYER_PROFILE()->Reset();
				GetMenuManager()->ResetMenuConfig();
				TutorialManager()->Reset();
				MissionDataManager()->SetSelectedChapter(-1); // --- TODO - remove selected chapter from MissionDataManager
				
			}
			auto profile = PLAYER_PROFILE();
			bool bFinishTutorial = false;
			if (CLevel::sSkipMenuTutorialNoDrone || CLevel::sSkipTutorialsNoArena)
			{
				auto menuTutorials = TutorialManager()->TutorialsOfType("menu");
				for (auto menuTutorial : menuTutorials)
				{
					auto shouldComplete = !CLevel::sSkipMenuTutorialNoDrone
						|| menuTutorial->id.find("tutorial_DroneOps") == std::string::npos;
					if (shouldComplete)
						shouldComplete = !CLevel::sSkipTutorialsNoArena
						|| menuTutorial->id.find("tutorial_Arena") == std::string::npos;
					if (shouldComplete)
					{
						profile->SetTutorialCompleted(menuTutorial->id.c_str(), true);
					}
				}
				bFinishTutorial = true;
			}
			if (CLevel::sSkipInGameTutorial)
			{
				auto ingameMenuTutorials = TutorialManager()->TutorialsOfType("ingame_menu");
				for (auto ingameMenuTutorial : ingameMenuTutorials)
					profile->SetTutorialCompleted(ingameMenuTutorial->id.c_str(), true);
				auto ingameTutorials = TutorialManager()->TutorialsOfType("ingame");
				for (auto ingameTutorial : ingameTutorials)
						profile->SetTutorialCompleted(ingameTutorial->id.c_str(), true);
				bFinishTutorial = true;
			}
			if (CLevel::sSetPlayer_BaseLevelMax)
			{
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_xp_more"));
				Json::Value data;
				data["currency"] = "base_level";
				data["value"] = 75;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
				
			}
			if (CLevel::sUnlockAllLevels)
			{
		
				if (!Application::GetInstance()->m_onlineActive)
				{
					// --- TODO: local unlock all
				}
				else
				{
					GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UnlockAllMissions);
				}

			}
			if (CLevel::sFinishAllTutorials)
			{
				auto menuTutorials = TutorialManager()->TutorialsOfType("menu");
				if (CLevel::sSkipMenuTutorialNoDrone)
					profile->SetTutorialCompleted("tutorial_DroneOps", true);
				else
				{
					for (auto menuTutorial : menuTutorials)
						profile->SetTutorialCompleted(menuTutorial->id.c_str(), true);
				}
				if (!CLevel::sSkipInGameTutorial)
				{
					auto ingameMenuTutorials = TutorialManager()->TutorialsOfType("ingame_menu");
					for (auto ingameMenuTutorial : ingameMenuTutorials)
						profile->SetTutorialCompleted(ingameMenuTutorial->id.c_str(), true);
					auto ingameTutorials = TutorialManager()->TutorialsOfType("ingame");
					for (auto ingameTutorial : ingameTutorials)
						profile->SetTutorialCompleted(ingameTutorial->id.c_str(), true);
				}
				bFinishTutorial = true;
			}

			if (bFinishTutorial)
			{
				profile->InvalidateTutorialStep();
				TutorialManager()->Reset();
				GetShopManager()->AcceptOffer("pvp_tutorial");

				// --- also make sure the assault is owned / equipped
				GetShopManager()->AcceptOffer("AR02");
				//hack for last transaction id in shop manager
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendActivateCredit(StringHash("AR02"));

			}
			GetOnlineManager()->GDPRResetCachedUserInfo();
			GetOnlineManager()->GDPRResetRegistrationFromCache();
			GetOnlineManager()->InitGDPRFlow();
			
			GetApplication()->GetDidomiInstance()->SetUserDisagreeToAll();
			GetOnlineManager()->GetReloginManager()->StartAHRelogin(false, RECONNECTING_STATE_UNLOAD_AND_FULL_RELOGIN);
		} } },
		{ ASEO_resetDidomi,			"ResetData Didomi",					{ []() {
			
			GetApplication()->GetDidomiInstance()->ResetData();
		} } },
		{ ASEO_none,				"Empty", "" },
		{ ASEO_skipTutorials,		"SkipTutorials(-drone) +Cons",	{ []() {
			auto profile = PLAYER_PROFILE();
			auto menuTutorials = TutorialManager()->TutorialsOfType("menu");
			for (auto menuTutorial : menuTutorials)
				profile->SetTutorialCompleted(menuTutorial->id.c_str(), true);
			auto ingameMenuTutorials = TutorialManager()->TutorialsOfType("ingame_menu");
			for (auto ingameMenuTutorial : ingameMenuTutorials)
				profile->SetTutorialCompleted(ingameMenuTutorial->id.c_str(), true);
			auto ingameTutorials = TutorialManager()->TutorialsOfType("ingame");
			for (auto ingameTutorial : ingameTutorials)
				if (ingameTutorial->id.find("_Drone") == std::string::npos)
					profile->SetTutorialCompleted(ingameTutorial->id.c_str(), true);
			profile->InvalidateTutorialStep();
			TutorialManager()->Reset();
			GetShopManager()->AcceptOffer("pvp_tutorial");

			// --- give in game consumables
			{
				const char* consumables[] = { "slow_time", "spotter", "piercing_bullet" };
				for (int i = 0; i < sizeof(consumables) / sizeof(*consumables); ++i)
				{
					Json::Value data;
					data["currency"] = consumables[i];
					data["value"] = 50;
					GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
				}
			}

			// --- also make sure the assault is owned / equipped
			GetShopManager()->AcceptOffer("AR02");
			//hack for last transaction id in shop manager
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendActivateCredit(StringHash("AR02"));

			GetOnlineManager()->GetReloginManager()->StartAHRelogin(false, RECONNECTING_STATE_UNLOAD_AND_FULL_RELOGIN);
		} } },
		{ ASEO_skipTutorials_noFTUE,		"SkipTutorials(-FTUE event)",{ []() {
			auto profile = PLAYER_PROFILE();
			auto menuTutorials = TutorialManager()->TutorialsOfType("menu");
			for (auto menuTutorial : menuTutorials)
			{
				if ((menuTutorial->id.find("tutorial_CoopEventPartOne") == std::string::npos) &&
					(menuTutorial->id.find("tutorial_CoopEventPartTwo") == std::string::npos))
					profile->SetTutorialCompleted(menuTutorial->id.c_str(), true);
			}
			auto ingameMenuTutorials = TutorialManager()->TutorialsOfType("ingame_menu");
			for (auto ingameMenuTutorial : ingameMenuTutorials)
			{
				if (ingameMenuTutorial->id.find("tutorial_CoopEventVictory") == std::string::npos)
					profile->SetTutorialCompleted(ingameMenuTutorial->id.c_str(), true);
			
			}
			auto ingameTutorials = TutorialManager()->TutorialsOfType("ingame");
			for (auto ingameTutorial : ingameTutorials)
				profile->SetTutorialCompleted(ingameTutorial->id.c_str(), true);
				
			GetShopManager()->AcceptOffer("pvp_tutorial");
			// --- also make sure the assault is owned / equipped
			GetShopManager()->AcceptOffer("AR02");
			//hack for last transaction id in shop manager
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendActivateCredit(StringHash("AR02"));

			profile->InvalidateTutorialStep();
			TutorialManager()->Reset();

			GetOnlineManager()->GetReloginManager()->StartAHRelogin(false, RECONNECTING_STATE_UNLOAD_AND_FULL_RELOGIN);
		} } },
		{ ASEO_skipAllTutorials,	"Skip All Tutorials",			{ []() {
			auto profile = PLAYER_PROFILE();
			auto menuTutorials = TutorialManager()->TutorialsOfType("menu");
			for (auto menuTutorial : menuTutorials)
				profile->SetTutorialCompleted(menuTutorial->id.c_str(), true);
			auto ingameMenuTutorials = TutorialManager()->TutorialsOfType("ingame_menu");
			for (auto ingameMenuTutorial : ingameMenuTutorials)
				profile->SetTutorialCompleted(ingameMenuTutorial->id.c_str(), true);
			auto ingameTutorials = TutorialManager()->TutorialsOfType("ingame");
			for (auto ingameTutorial : ingameTutorials)
				profile->SetTutorialCompleted(ingameTutorial->id.c_str(), true);
			profile->SetTutorialCompleted("tutorial_MyBaseSwipe", true);
			profile->InvalidateTutorialStep();
			TutorialManager()->Reset();
			GetShopManager()->AcceptOffer("pvp_tutorial");

			// --- also make sure the assault is owned / equipped
			GetShopManager()->AcceptOffer("AR02");
			//hack for last transaction id in shop manager
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendActivateCredit(StringHash("AR02"));

			GetOnlineManager()->GetReloginManager()->StartAHRelogin(false, RECONNECTING_STATE_UNLOAD_AND_FULL_RELOGIN);
		} } },
		{ ASEO_replayLevels,		"Replay completed levels",		GetSettingsMgr()->m_bCheatReplayLevels },
		{ ASEO_debugNamesForWeapons,"Debug names for weapons",		GetSettingsMgr()->m_bShowDebugNameForWeapons },
		{ ASEO_unlockNextChapter,	"Unlock Next Chapter",			{ []() {
			if (!Application::GetInstance()->m_onlineActive)
			{
				// --- TODO: local unlock all
			}
			else
			{
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UnlockNextChapter);
			}
		} } },
		{ ASEO_unlockNextDroneOpsChapter,	"Unlock Next DroneOps Chapter",{ []() {
			if (!Application::GetInstance()->m_onlineActive)
			{
				// --- TODO: local unlock all 
			}
			else
			{
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UnlockNextDroneOpsChapter);
			}
		} } },
		{ ASEO_unlockLevels,		"Unlock All Chapters",			{ []() {
			if (!Application::GetInstance()->m_onlineActive)
			{
				// --- TODO: local unlock all
			}
			else
			{
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UnlockAllMissions);
			}
		} } },
		{ ASEO_resetRegistration,		"Reset China Registration",			{ []() {
			
			PLAYER_PROFILE()->SetUserAge(0);
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendUserAge(0, PLAYER_PROFILE()->GetUserGender(), "");
			ADS()->UpdateUserGender();

			GetOnlineManager()->ResetGDPRFlow();
			GetOnlineManager()->ResetAgeInGDPRFlow(true);
			GetOnlineManager()->GDPRResetCachedUserInfo();
			GetOnlineManager()->GDPRResetRegistrationFromCache();
			GetOnlineManager()->InitGDPRFlow();
			GetOnlineManager()->GetReloginManager()->StartAHRelogin();//login again with anonymous    
		} } },
		{ ASEO_shopMenuOverride,		"Shop menu override: 0:None 1:New 2:Old",	{ g_newShopOverride,  0, 2, []() {
		} } },
		{ ASEO_newDailyRewardPopup,			"New Daily Reward Popup: 0:None 1:New 2:Old",	{ isNewDailyReward, 0, 2, []() {
		} } },
		{ ASEO_DailyV2UnlockTime,		"Set New User Unlock Daily V2 in Minutes",		{ fCheatDailyV2UnlockTime,  0, 60, []() {
			Json::Value data;
			data["min"] = (int)_round(fCheatDailyV2UnlockTime * 60.0f);
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::NewUserUnlockDailyV2, data);
		} } },
		{ ASEO_fpsInfo,				"FPS Info Small",				CLevel::sFPSInfoSmall },
		{ ASEO_dInfo,				"Debug Info",					CLevel::s_bDebugInfo },
		{ ASEO_toggleKillAllButton,	"Toggle Kill All Button",		CLevel::s_bShowKillAllBtn },
		{ ASEO_mpInfo,				"Mp Info",						CLevel::s_bMpDebugInfo },
		{ ASEO_damagePerClip,		"Show damage per clip",			GetSettingsMgr()->m_bDamagePerClip },
		{ ASEO_ToggleDominationSettings,	"Switch to debug (quick) domination settings",	{ GetClansManager()->s_bCheatDominationSettingsActive, []() {
			if (GetClansManager()->MyClan())
				GetClansManager()->Cheat_ToggleDominationSettings();
		} } },
		{ ASEO_profileAttack,		"Show attack button in profile",GetSettingsMgr()->m_bButtonAttackProfile },
		{ ASEO_showPlayerInfo,		"Show Player Info",				CLevel::s_bShowPlayerInfo },
		{ ASEO_showAllDefLogs,		"Show hidden defense logs",		GetSettingsMgr()->m_bCheatShowAllDefLogs },
		{ ASEO_showNPCDamage,			"NPC Damage&Health",		CLevel::s_bShowNPCDamage },
		{ ASEO_showNPCDamageTaken,		"NPC Damage Taken",			CLevel::s_bShowNPCDamageTaken },
		{ ASEO_showOpponentLocation,"Show opponent geolocation",	GetSettingsMgr()->m_bCheatShowOpponentLocation },


		{ ASEO_getNextSevenDaysLogin, "Get Next 7 Days Reward",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::GetNext7DaysRw);
		} } },
		{ ASEO_resetSevenDaysLogin, "Reset Seven Days Login",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetSevenDaysLogin);
		} } },
		{ ASEO_resetSevenDaysSpecial, "Reset Seven Days Special",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetSevenDaysSpecial);
		} } },
		{ ASEO_getNextSevenDaysSpecial, "Get Next Seven Days Special",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::GetNextSevenDaysSpecial);
		} } },
		{ ASEO_resetDailyChallenges,"Reset Warlord",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetDailyChallenges, data);
		} } },
		{ ASEO_nextDailyChallenge,	"Next Daily Challenge",			{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::NextDailyChallenge, data);
		} } },
		{ ASEO_resetDailyReward,	"Reset Daily Reward",			{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetDailyReward, data);
		} } },
		{ ASEO_resetDailyRewardV2,	"Reset Daily Reward V2",			{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetDailyRewardV2, data);
		} } },
		{ ASEO_getDailyRewardV2,		"Get Next Daily Reward V2",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::GetDailyRewardV2, data);
		} } },
		{ ASEO_skipDailyRewardV2,		"Skip 1 Daily Reward V2",				{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SkipDailyRewardV2, data);
		} } },
		{ ASEO_skipMonthDailyRewardV2,		"Skip To Next Month Daily Reward V2",				{ []() {
			Json::Value data;
			//auto now = GetShopManager()->GetServerTime();
			//auto nowTm = gmtime(&now);
			//auto tm_mday = nowTm->tm_mday;
			data["skipDay"] = 31;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SkipMonthDailyRewardV2, data);
		} } },
		{ ASEO_selectDailyReward,	"Select Daily Reward",			GetSettingsMgr()->m_dailyRewardNumber }, // --- TODO: dateModifier->setMember("measureUnit", "#");
		{ ASEO_getDailyReward,		"Get Daily Reward",				{ []() {
			int day = (int)(GetSettingsMgr()->m_dailyRewardNumber * 100 + 0.5f);
			if (day > 0)
			{
				Json::Value data;
				data["day"] = day;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::GetDailyReward, data);
			}
		} } },

		{ ASEO_reset_coop,			"Reset ladder event",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCoop);
		} } },
		{ ASEO_reset_miniops,		"Reset seasonal event",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetMiniops);
		} } },
		{ ASEO_getSubscriptionReward,	"Get Subscription Reward",	{ []() {
			int day = (int)(GetSettingsMgr()->m_dailyRewardNumber * 100 + 0.5f);
			if (day > 0)
			{
				Json::Value data;
				data["day"] = day;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::GetSubscriptionReward, data);
			}
		} } },
		{ ASEO_increaseDayRateGame,		"Increase Day Rate Game",	{ []() {
			GetSettingsMgr()->m_counterServerRateGame++;

			Json::Value data;
			data["counterRateGame"] = GetSettingsMgr()->m_counterServerRateGame;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::GetCounterRateGame, data);
		} } },
		{ ASEO_notificationsCheat,		"Cheat Notifications",				GetSettingsMgr()->m_bNotificationsCheat },

#if USE_MULTIPLAYER_TEST_DATA
		{ ASEO_pvpTestData,				"Show pvp test data",				GetShopManager()->GetMultiplayerTestData().menuActive },
#endif // USE_MULTIPLAYER_TEST_DATA
		//TODO - Reset ladder event weapon upgrades

		{ ASEO_copyAnon,			"Copy Anon 2 Clipboard",		{ []() {
			void CopyAnonToClipboard();
			CopyAnonToClipboard();
		} } },
		{ ASEO_toggleQAGC,			"Toggle QAGC",					{bExistsQAGCFile, []() {
			DLC()->ToggleQAGCFile();
		} } },
		{ ASEO_deleteDLC,			"Delete DLC",					{ []() {
			DLC()->DeleteDownloadedLocations();
		} } },
		{ ASEO_bountyReset,			"Reset global hunt event",		{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetBountyEvent);
		} } },
		{ ASEO_short_squad_ops,		"Short Squad Ops",				{bShortSquadOps, []() {
			Json::Value data;
			data["enable"] = bShortSquadOps;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ToggleShortSquadOps, data);
		} } },
		{ ASEO_skip_time_free_spin,	"Reset time for Spin Wheel - Free",	{ []() {
			Json::Value data;
			data["spin_type"] = antihackgs::SpinWheelType::Free;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SkipFreeSpinTime, data);
		} } },
		{ ASEO_skip_time_free_spin_tle,	"Reset time for Spin Wheel - TLE",	{ []() {
			Json::Value data;
			data["spin_type"] = antihackgs::SpinWheelType::FreeTle;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SkipFreeSpinTime, data);
		} } },
		{ ASEO_skip_time_free_spin_outpost,	"Reset time for Spin Wheel - OutPost",	{ []() {
			Json::Value data;
			data["spin_type"] = antihackgs::SpinWheelType::FreeOutpost;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SkipFreeSpinTime, data);
		} } },
		{ ASEO_startPvpMission1,	"Start pvp level 1",			{ []() {
			Map_startMission(MissionUid(0, 17, 0));
		} } },
		{ ASEO_virtuaCopMission,	"Start virtuaCop mission",		{ []() {
			Map_startMission(MissionUid(22, 18, 0));
		} } },
		{ ASEO_add_defender_score,	"Add defender score to leaderboard",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::AddDefenderScore);
		} } },
		{ ASEO_test_defender_HOF_rewards,	"Test last Defender season HOF rewards",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SendLastDefenderSeasonHOFRewards);
		} } },
		{ ASEO_test_defender_personal_milestone_rewards,	"Test_defender_personal_milestone_rewards",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::TestDefenderPersonalMilestoneRewards);
		} } },
		{ ASEO_reset_defender_personal_milestone_rewards,	"Reset_defender_personal_milestone_rewards",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetDefenderPersonalMilestoneRewards);
		} } },

		//TODO cheat switch (switch, default on, that displays a "red something" when cheats have been used on a profile)

		//old debug/cheats ------------------------------------------------------------
		{ ASEO_none,				"",								{ "Old stuff from here on out" } },
		{ ASEO_observer,			"Observer mode",				GetSettingsMgr()->m_observerMode },
		{ ASEO_TriggerSocialConnectivityError,	"Trigger Social Connect. Error",	{ []() {
			//if (GetSocialLibManager()->m_loginSNS[SNS_FACEBOOK].isLoggedIn)
			{
				std::string errorMsg("social connectivity errors - CLIENT_SNS_FACEBOOK");
				sociallib::CLIENT_SNS_INTERFACE->ReturnFakeError(sociallib::ClientSNSEnum::CLIENT_SNS_FACEBOOK, sociallib::SNSRequestTypeEnum::SNS_REQUEST_TYPE_LOGIN, errorMsg);
			}
#if !defined(OS_ANDROID)
			//if (GetSocialLibManager()->m_loginSNS[SNS_GAME_CENTER].isLoggedIn)
			{

				std::string errorMsg("social connectivity errors - CLIENT_SNS_GAME_CENTER");
				sociallib::CLIENT_SNS_INTERFACE->ReturnFakeError(sociallib::ClientSNSEnum::CLIENT_SNS_GAME_CENTER, sociallib::SNSRequestTypeEnum::SNS_REQUEST_TYPE_LOGIN, errorMsg);
			}
#else
			//if (GetSocialLibManager()->m_loginSNS[SNS_GAME_API].isLoggedIn)
			{

				std::string errorMsg("social connectivity errors - CLIENT_SNS_GAME_API");
				sociallib::CLIENT_SNS_INTERFACE->ReturnFakeError(sociallib::ClientSNSEnum::CLIENT_SNS_GAME_API, sociallib::SNSRequestTypeEnum::SNS_REQUEST_TYPE_LOGIN, errorMsg);
			}
#endif // defined(OS_ANDROID)
		} } },
		{ ASEO_none,				"User credential",				{ nullptr, []() {
			auto myProfile = GetOnlineManager()->GetUserSocialManager()->GetMyProfile();
			if (myProfile)
			{
				if (myProfile->m_user)
					return std::string(myProfile->m_user->credential());
				else
					return std::string("OsirisUser object is NULL !");
			}
			else
			{
				return std::string("No osiris user profile loaded !");
			}
		} } },
		{ ASEO_none,				"Country code info",			{ nullptr, []() {
			std::string countryCode; countryCode.reserve(128);
			std::string deviceCountry = GetApplication()->GetCountryCode();
			glf::Snprintf(&countryCode[0], countryCode.capacity(), "Country code: %s", deviceCountry.c_str());
			return countryCode;
		} } },
		{ ASEO_none,				"BPCounter",					{ nullptr, []() {
			std::string battlePackCounterStr; battlePackCounterStr.reserve(128);
			glf::Snprintf(&battlePackCounterStr[0], battlePackCounterStr.capacity(), "BPCounter: %d", PLAYER_PROFILE()->GetBPFreeAdCounter());
			return battlePackCounterStr;
		} } },
		{ ASEO_none,				"CqPower",						{ nullptr, []() {
			return std::string("Conquest power: ") + std::to_string(PLAYER_PROFILE()->m_ConquestPower);
		} } },
		{ ASEO_none,				"CqBracket",					{ nullptr, []() {
			return std::string("Conquest bracket: ") + std::to_string(PLAYER_PROFILE()->m_ConquestBracket);
		} } },

		{ ASEO_StartTestLevel,	"Start test level",				{ []() {
			Application::GetInstance()->RequestLoadTestLevel();
		} } },
		{ ASEO_shardQuestComplete,   "Enable cheat event shard Quest complete",  isCheatShardQuestCompleteEnable },
		{ ASEO_shardQuestResetAllProgress,   "Reset event shard quest progress",  { []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ShardQuestResetProgress);
		} } },
		{ ASEO_piggyBankOpenCommendationPopup,   "Open piggybank commendation popup",  { []() {
			GetMenuManager()->ShowPiggyBankCommendationPopup();
		} } },
		{ ASEO_piggyBankFillUp,   "Fill up piggybank",  { []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FillPiggyBank);
		} } },
//GE NTT ADD 
#if defined(OS_ANDROID)
		{ ASEO_sendlogfabric,	"Send requied for fabric",{ []() {
			DebugCrash();
		} } },
#endif
//GE NTT END
	},
	{},
	}, // --- end debug category

		////////////////////////////////////////////////////////////////////////////////////////////////////
		{ "about", "INFO", DEBUG_DATA_INFO,
	{
		{ ASEO_none,				"Info",							{ nullptr, []() {
			char AppVersion[8] = "";
			ApplicationInfo::GetVersion(AppVersion, 8);

			char SvnVersion[128] = "unknown";
			ApplicationInfo::GetSvnRevision(SvnVersion, 128);

			char Version[1024];
			sprintf(Version, "App_Version: %s%s\nDatacenter: %s\n\nSvn_Version: %s\n\n%s - %s - %s\nGeometry level:%d, metal low = %s\nGPU: %s\nnScreenRes: %dx%d\nRTTSize: %dx%d(f=%.2f)\nSmallRTTSize: %dx%d(f=%.2f)",
				AppVersion,
				DLC()->IsDlcEnabled() ? "_DLC" : "",
				GetGaia()->GetDataCenter().c_str(),
				SvnVersion,
				GetDeviceOptions()->GetGameOptionsCPUProfile().c_str(),
				GetDeviceOptions()->GetGameOptionsMEMProfile().c_str(),
				GetDeviceOptions()->GetGameOptionsGPUProfile().c_str(),
				GetDeviceOptions()->GetGeometryLevel(), GetDeviceOptions()->GetMetalLowProfile() ? "true" : "false",
				GetDeviceOptions()->GetGpuName().c_str()
				, SCR_W, SCR_H
				, GetDeviceOptions()->GetRTTWidth(), GetDeviceOptions()->GetRTTHeight()
				, GetDeviceOptions()->GetRTTScaleFactor()
				, GetDeviceOptions()->GetSmallRTTWidth(), GetDeviceOptions()->GetSmallRTTHeight()
				, GetDeviceOptions()->GetSmallRTTScaleFactor()
			);
			return std::string(Version);
		} } },
	},
	{}, 
	}, // --- end about category

		////////////////////////////////////////////////////////////////////////////////////////////////////
		{ "cheats", "CHEATS", DEBUG_DATA_CHEATS | DEBUG_DATA_CHEATS_ENDLEVEL, {},
	{
			{ "currency", "Currency", DEBUG_DATA_CHEATS, {
		{ ASEO_freeSc,				"Free sc",						{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_sc"));
		} } },
		{ ASEO_reset_sc,			"Reset sc",						{ []() {
			Json::Value data;
			data["currency"] = "sc";
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_freeHc,				"Free hc",						{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_hc"));
		} } },
		{ ASEO_reset_hc,			"Reset hc",{ []() {
			Json::Value data;
			data["currency"] = "hc";
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_freeDust,			"Fill Vault",					{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_GOLD;
			data["value"] = INT_MAX;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_reset_dust,			"Reset dust",					{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_GOLD;
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },			
		{ ASEO_consumeDust,			"Consume dust",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("consume_dust"));
		} } },
		{ ASEO_freeMoreHc,			"More free hc",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("more_free_hc"));
		} } },
		{ ASEO_freeClanCash,		"Free clan cash",				{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_clan_cash"));
		} } },
		{ ASEO_consumeClanCash,		"Reset clan cash",				{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_CLAN_CASH;
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		
		{ ASEO_free_neutronium,		"Free neutronium",				{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_neutronium"));
		} } },
		{ ASEO_reset_neutronium,	"Reset neutronium",				{ []() {
			Json::Value data;
			data["currency"] = "neutronium";
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::RefillNeutronium, data);
		} } },
		{ ASEO_free_survival_honor,	"Free survival honor",			{ []() {
			Json::Value data;
			data["item"] = SHOP_ITEM_SURVIVAL_HONOR;
			data["qty"] = 100000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_reset_survival_honor,"Reset survival honor",			{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_SURVIVAL_HONOR;
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_free_survival_spoils,	"Free survival spoils",			{ []() {
			Json::Value data;
			data["item"] = SHOP_ITEM_SURVIVAL_SPOILS;
			data["qty"] = 100000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_reset_survival_spoils,	"Reset survival spoils",			{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_SURVIVAL_SPOILS;
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_freeRaidTickets,		"Free raid tickets",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_raid_tickets"));
		} } },
		{ ASEO_freeBossEventKeys,	"Free boss event keys",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_BOSS_EVENT_KEY;
			data["qty"] = 50;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_giveAllCurrencies,	"Give all currencies",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_sc"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_hc"));
			auto safeDustCredit = GetShopManager()->FindShopItem("safe_dust");
			auto safeDust = GetShopManager()->GetShopItemCurrentValue(safeDustCredit);
			auto safeDustMax = GetShopManager()->GetMaxQty(safeDustCredit);
			if(safeDust<safeDustMax)
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_dust"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_clan_cash"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_raid_tickets"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_event_keys"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_neutronium"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_diamondpoints"));
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_vip_tickets"));

			Json::Value data;
			data["item"] = SHOP_ITEM_ARENA_TOKEN;
			data["qty"] = 5000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
			Json::Value data1;
			data1["item"] = SHOP_ITEM_SKIRMISH_TOKEN;
			data1["qty"] = 5000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data1);
			Json::Value data2;
			data2["item"] = SHOP_ITEM_SHARD_CURRENCY;
			data2["qty"] = 5000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data2);
		} } },
		{ ASEO_resetAllCurrencies,	"Reset all currencies",			{ []() {
			//currencies
			const char* consumables[] = { "sc", "hc", "safe_dust", "clan_cash", "raid_ticket", "green_key", "yellow_key", "purple_key", SHOP_ITEM_ARENA_TOKEN, SHOP_ITEM_SKIRMISH_TOKEN, SHOP_ITEM_SHARD_CURRENCY };
			for (int i = 0; i < sizeof(consumables) / sizeof(*consumables); ++i)
			{
				Json::Value data;
				data["currency"] = consumables[i];
				data["value"] = 0;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
			//reset neutronium
			{
				Json::Value data;
				data["currency"] = "neutronium";
				data["value"] = 0;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::RefillNeutronium, data);
			}
		} } },
		{ ASEO_giveEnergy,			"Give all energy",				{ []() {
			const char* consumables[] = { "energy", "arena_energy", "demon_energy", "survival_energy", "skirmish_energy"};
			for (int i = 0; i < sizeof(consumables) / sizeof(*consumables); ++i)
			{
				Json::Value data;
				data["item"] = consumables[i];
				data["qty"] = 100;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
			}
		} } },
		{ ASEO_consumeAllEnergy,	"Reset all energy",				{ []() {
			//GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("consume_energy"));
			const char* consumables[] = { "energy", "arena_energy", "demon_energy", "survival_energy", "skirmish_energy"};
			for (int i = 0; i < sizeof(consumables) / sizeof(*consumables); ++i)
			{
				Json::Value data;
				data["currency"] = consumables[i];
				data["value"] = 0;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
		} } },
		{ ASEO_consumeEnergy,		"Consume 1 energy",				{ []() {
			if (!GetShopManager())
				return;
			int n = GetShopManager()->GetShopItemCurrentValue(SHOP_ITEM_ENERGY);
			if (n >= 1)
			{
				Json::Value data;
				data["currency"] = SHOP_ITEM_ENERGY;
				data["value"] = n - 1;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
		} } },
		{ ASEO_consumeArenaEnergy,	"Consume 1 Arena energy",			{ []() {
			if (!GetShopManager())
				return;
			int n = GetShopManager()->GetShopItemCurrentValue("arena_energy");
			if (n >= 1)
			{
				Json::Value data;
				data["currency"] = "arena_energy";
				data["value"] = n - 1;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
		} } },
		{ ASEO_freeBountyCurrencies,"Free global hunt currency",	{ []() {
			const char* consumables[] = { SHOP_ITEM_BOUNTY_TOKEN, SHOP_ITEM_BOUNTY_STAR };
			for (int i = 0; i < sizeof(consumables) / sizeof(*consumables); ++i)
			{
				Json::Value data;
				data["item"] = consumables[i];
				data["qty"] = 10000;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
			}
		} } },
		{ ASEO_bountyMapFragments,	"Global hunt map fragments",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CompleteCurrentBountyChapterMap);
		} } },
		{ ASEO_bountyBombs,			"Global hunt bombs",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CompleteCurrentBountyChapterBombs);
		} } },
#ifdef ENABLE_KARMA
		{ ASEO_freeKarma,				"Free karma",				{ []() {
			auto shmgr = GetShopManager();
			const auto& mp_data = shmgr->GetMultiplayerData();
			Json::Value data;
			data["item"] = "karma";
			data["qty"] = mp_data.GetKarmaGaugePoints() *  mp_data.GetKarmaGaugeMaxNb();
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
			Home_SendHomeScreenData();
		} } },
		{ ASEO_reset_karma,			"Reset karma",					{ []() {
			Json::Value data;
			data["currency"] = "karma";
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			Home_SendHomeScreenData();
		} } },
#endif
		{ ASEO_freeEventItems,		"Free event items",				{ []() {
			auto ev = SemEventManager::Instance()->GetUniqueEvent(EVENT_TYPE_MINIOPS);
			if (!ev)
				return;
			auto eventData = EventsDataManager()->GetMiniopsEventData(ev->GetPresetHash());

			Json::Value data;
			data["item"] = GetShopManager()->Hash2Name(eventData.EventItem());
			data["qty"] = 10000;

			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_resetEventItems,		"Reset event items",			{ []() {
			auto ev = SemEventManager::Instance()->GetUniqueEvent(EVENT_TYPE_MINIOPS);
			if (!ev)
				return;
			auto eventData = EventsDataManager()->GetMiniopsEventData(ev->GetPresetHash());

			Json::Value data;
			data["currency"] = GetShopManager()->Hash2Name(eventData.EventItem());
			data["value"] = 0;

			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_crash,				"Crash",						{ []() {
			int* crash = nullptr;
			*crash = 42;
		} } },
		}, {}, }, // --- end currency subcategory

		///////////////////////////////////////////////////////
			{ "weapons", "Weapons", 0, {

		{ ASEO_unlockSeriesWeapons,		"Unlock Series Weapons",			{ []() {
			Json::Value data;
			data["category"] = "series";
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::OwnAllWeapons, data);
		} } },
		{ ASEO_unlockSkinnedWeapons,	"Unlock Skinned Weapons",			{ []() {
			Json::Value data;
			data["category"] = "skinned";
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::OwnAllWeapons, data);
		} } },
		{ ASEO_skinnedToHighestUnlocked,"Set Skinned To HighestUnlocked",	{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetHighestUnlocked);
		} } },
		{ ASEO_fullyUpgradeWeapons,		"Fully upgrade weapons",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FullyUpgradeWeapons);
		} } },
		{ ASEO_fullyUpgradeDrones,		"Fully upgrade drones",{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FullyUpgradeDrones);
		} } },
		{ ASEO_resetSeriesWeapons,		"Reset series weapons",				{ []() {
			Json::Value data;
			data["category"] = "series";
			data["reset"] = true;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::OwnAllWeapons, data);
		} } },
		{ ASEO_resetSkinnedWeapons,		"Reset skinned weapons",			{ []() {
			Json::Value data;
			data["category"] = "skinned";
			data["reset"] = true;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::OwnAllWeapons, data);
		} } },
		{ ASEO_freeParts,				"Give craft parts",					{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_CRAFT_PART;
			data["qty"] = 200;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_freeDroneCraftParts,		"Give drones craft parts",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_DRONE_CRAFT_PART;
			data["qty"] = 200;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_reset_craft_parts,		"Reset craft parts",				{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_CRAFT_PART;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetItemsFromCategory, data);
		} } },
		{ ASEO_reset_drone_craft_parts,	"Reset drone craft parts",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_DRONE_CRAFT_PART;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetItemsFromCategory, data);
		} } },
		{ ASEO_give_weapon_parts,		"Give weapon parts & assemble",		{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_WEAPON;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::AssembleParts, data);
		} } },
		{ ASEO_give_drone_parts,		"Give drone parts & assemble",		{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_DRONE_WEAPON;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::AssembleParts, data);
		} } },
		{ ASEO_freeWeaponParts,			"Free weapon parts",				{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeWeaponParts);
		} } },
		{ ASEO_freeCommandersParts,			"Free commanders parts",				{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeCommandersParts);
		} } },
		{ ASEO_reset_commanders_parts,	"Reset commanders parts",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_COMMANDER_PART;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetItemsFromCategory, data);
		} } },

		}, {}, }, // --- end weapons subcategory

		///////////////////////////////////////////////////////
			{ "consumables", "Consumables", 0, {

		{ ASEO_freeSquadOpsBoosts,		"Free squad ops boosts",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_SO_BOOST;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_giveBattlepacks,			"Give battlepacks",{ []() {
			Json::Value data;
			data["category"] = SHOP_OFFER_CATEGORY_GACHA;
			data["qty"] = 50;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);

			/*data["currency"] = "lucky_key"; //surplus key
			data["value"] = 50;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);*/
		} } },		
		{ ASEO_freeCards,				"Give Squadmates&XPCards",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeCards);
		} } },
		{ ASEO_reset_squadmates,		"Reset squadmates",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetSquadmates);

			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_PLAYER_CARD_PART;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetItemsFromCategory, data);
		} } },
		{ ASEO_upgradeSquadmates,		"Upgrade squadmates",				{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UpgradeSquadmates);
		} } },
		{ ASEO_upgradeSquadmatesLower,	"Upgrade squadmates 1 lower",		{ []() {
			Json::Value data;
			data["tier"] = -1;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UpgradeSquadmates, data);
		} } },
		{ ASEO_justFreeCards,			"Give XPCards",						{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::JustFreeCards);
		} } },
		{ ASEO_giveConsumables,			"Give ingame consumables",			{ []() {
			const char* consumables[] = { "slow_time", "spotter", "piercing_bullet" };
			Json::Value data;
			data["value"] = 50;
			for (auto item : consumables)
			{
				data["currency"] = item;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
		} } },
		{ ASEO_reset_consumables,		"Reset consumables",				{ []() {
			const char* consumables[] = { "slow_time", "spotter", "piercing_bullet" };
			Json::Value data;
			data["value"] = 0;
			for (auto item : consumables)
			{
				data["currency"] = item;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
		} } },
		{ ASEO_giveConsumablesCheat,		"Give ingame consumables (cheat)",{ []() {
			const char* consumables[] = { "slow_time", "spotter", "piercing_bullet" };
			Json::Value data;
			data["value"] = 500;
			data["overmax"] = true;	// --- bypasses all checks, use with caution!
			for (auto item : consumables)
			{
				data["currency"] = item;
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
			}
		} } },

		{ ASEO_freePvpBoosts,			"Give pvp boosts",					{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_PVP_BOOST;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_reset_pvp_boosts,		"Reset PVP boosts",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetPvpBoosts);
		} } },	

		{ ASEO_freePvpConsumables,			"Give pvp consumables",			{ []() {
			Json::Value data; //ceg arena
			data["category"] = SHOP_ITEM_CATEGORY_PVP_CONSUMABLE;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_reset_pvp_consumables,		"Reset PVP consumables",		{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetPvpConsumables);
		} } },

		{ ASEO_DeathRattleFlashbang,	"Flashbang DeathRattle",		CDebuffComponent::m_sAllowedDebuffs[k_RevengeType_Flashbang] },
		{ ASEO_DeathRattleDisruptor,	"Disruptor DeathRattle",		CDebuffComponent::m_sAllowedDebuffs[k_RevengeType_Disruptor] },
		{ ASEO_DeathRattleHealingSphere,"Heal Sphere DeathRattle",		CDebuffComponent::m_sAllowedDebuffs[k_RevengeType_HealSphere] },

		{ ASEO_FreeMods,				"Free mods",						{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeMods);
		} } },
		{ ASEO_ResetMods,				"Reset mods",						{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetMods);
		} } },

		{ ASEO_FreeArenaBoosts,			"Free arena boosts",				{ []() {
			Json::Value data;
			data["qty"] = 20;

			data["item"] = SHOP_ITEM_GIVE_ARENA_DOUBLE_REWARD;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);

			data["item"] = SHOP_ITEM_GIVE_UNLIMITED_ARENA_ENERGY;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_ResetArenaBoosts,		"Reset arena boosts",				{ []() {
			Json::Value data;
			data["value"] = 0;

			data["currency"] = SHOP_ITEM_GIVE_ARENA_DOUBLE_REWARD;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);

			data["currency"] = SHOP_ITEM_GIVE_UNLIMITED_ARENA_ENERGY;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },

		}, {}, }, // --- end consumables subcategory

		///////////////////////////////////////////////////////
			{ "profile", "Profile", 0, {

		//TODO Expire all rentals
		//TODO Expire shields and their cooldown

		{ ASEO_freeXP_10000,			"Give 10k XP",						{ []() {
			Json::Value data;
			data["item"] = "xp";
			data["qty"] = 10000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_freeXP_more,				"Give 10m XP",						{ []() {
			Json::Value data;
			data["item"] = "xp";
			data["qty"] = 10000000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_freeXP_even_more,		"Give 500m XP",						{ []() {
			Json::Value data;
			data["item"] = "xp";
			data["qty"] = 500000000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_freeXP,					"Upgrade to max level",				{ []() {
			Json::Value data;
			data["item"] = "xp";
			data["qty"] = 280000000;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItems, data);
		} } },
		{ ASEO_upgradePVPBase,			"Upgrade base level",				{ []() {
			Json::Value data;
			data["currency"] = "base_level";
			data["value"] = 70;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_freeCamos,				"Give all camos",					{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_CAMO;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_freeFlags,				"Give all flags",					{ []() {
			// --- own them all
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_PVP_FLAG;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);

			// --- and equip a random one (or none)
			auto flagsData = GetShopManager()->GetMultiplayerData().PvpFlagsData().asBinaryStruct();
			int rnd = 0; // TODO std::uniform(0, flagsData.size());
			auto flagData = flagsData.begin();
			std::advance(flagData, rnd);
			PLAYER_PROFILE()->SetPvpFlag(flagData.valid() ? StringHash(flagData["ID"].asCString()) : StringHash(0));
		} } },
		{ ASEO_freeBaseSkins,			"Give all BaseSkins",				{ []() {
			// --- own them all
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_BASE_SKIN;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand( AHCheatCommand::FreeItemsFromCategory, data );
		} } },
		{ ASEO_give_drones,				"Give drones and turrets",{ []() {
			Json::Value data;
			data["qty"] = 50;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetDrones, data);
		} } },
		{ ASEO_reset_drones,			"Reset drones",						{ []() {
			Json::Value data;
			data["qty"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetDrones, data);
		} } },
		{ ASEO_expire_drones,			"Expire drones",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ExpireDrones);
		} } },

		{ ASEO_reset_companion_drone,	"Reset companion drone",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_1,	"Companion drone tier 1",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 1;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_2,	"Companion drone tier 2",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 2;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_3,	"Companion drone tier 3",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 3;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_4,	"Companion drone tier 4",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 4;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_5,	"Companion drone tier 5",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 5;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_6,	"Companion drone tier 6",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 6;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_companion_drone_tier_7,	"Companion drone tier 7",			{ []() {
			Json::Value data;
			data["credit"] = StringHash("companion_classic").GetHash();
			data["tier"] = 7;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetCompanionDrone, data);
		} } },
		{ ASEO_GivePlayerDrone,			"Give player drone",					{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_PLAYER_DRONE;
			data["value"] = 1;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_ResetPlayerDrone,			"Reset player drone",					{ []() {
			Json::Value data;
			data["currency"] = SHOP_ITEM_PLAYER_DRONE;
			data["value"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },

		{ ASEO_randomAvatarModule,		"Random avatar module",				{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_AVATAR_MODULE;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::RandomItemsFromCategory, data);

			// --- cheat for triggering GearStatus tracking event, since it's sent once daily - 8668912
			GLOT()->SendGearStatus(true);
		} } },
		{ ASEO_freeAvatarModules,		"Give all avatar modules",			{ []() {
			// --- own them all
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_AVATAR_MODULE;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);

			// --- cheat for triggering GearStatus tracking event, since it's sent once daily - 8668912
			GLOT()->SendGearStatus(true);
		} } },
		{ ASEO_freeBulletSkins,			"Free all bullet skins",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_BULLET_SKIN;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_resetAvatarModules,		"Reset all avatar modules",			{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_AVATAR_MODULE;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetItemsFromCategory, data);
		} } },
		{ ASEO_unlockRandomAchievement,	"Unlock Random Achiev",				{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UnlockRandomAchievement);
		} } },
		{ ASEO_accelerateAchievements,	"Accelerate Achievements",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::AccelerateAchievements);
		} } },
		{ ASEO_unlockAchievements,		"Unlock All Achievements",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UnlockAllAchievements);
		} } },
		{ ASEO_freeXP_more,				"even more XP",						{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAcceptOffer(StringHash("free_xp_more"));
		} } },
		{ ASEO_reset_shield,			"Reset shield",						{ []() {
			Json::Value data;
			data["duration"] = 0;
			data["cooldown"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetShield, data);
		} } },
		{ ASEO_expireAllRentals,			"Expire all rentals",{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ExpireAllRentals);
		} } },
		{ ASEO_reset_time_spent,		"Reset time spent",					{ []() {
			Json::Value data;
			data["reset"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatTimeSpent, data);
		} } },
		{ ASEO_add_time_spent_1m,		"Time spent +1m",					{ []() {
			Json::Value data;
			data["set"] = 60;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatTimeSpent, data);
		} } },
		{ ASEO_add_time_spent_10m,		"Time spent +10m",					{ []() {
			Json::Value data;
			data["set"] = 600;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatTimeSpent, data);
		} } },
		{ ASEO_add_time_spent_1h,		"Time spent +1h",					{ []() {
			Json::Value data;
			data["set"] = 3600;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatTimeSpent, data);
		} } },
		{ ASEO_add_time_spent_1d,		"Time spent +1d",					{ []() {
			Json::Value data;
			data["set"] = 86400;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatTimeSpent, data);
		} } },
		{ ASEO_resetBattlepass,			"Reset battlepass",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetBattlepass);
		} } },
		{ ASEO_completeBattlepassMilestone,			"Complete battlepass milestone",{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CompleteBattlepassMilestone);
		} } },
		{ ASEO_completeBattlepassMilestone1,			"Complete battlepass milestone to 99%",{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CompleteBattlepassMilestone1);
		} } },

		{ ASEO_freeHoloBadges,			"Give holo-badges",					{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_HOLO_BADGE;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeItemsFromCategory, data);
		} } },
		{ ASEO_resetHoloBadges,			"Reset holo-badges",				{ []() {
			Json::Value data;
			data["category"] = SHOP_ITEM_CATEGORY_HOLO_BADGE;
			data["qty"] = 0;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetItemsFromCategory, data);
		} } },

		{ ASEO_force_China_Gacha,	"Force China Gacha",					s_forceChinaGacha },

		{ ASEO_setSubscriptionDayOffset,	"Set Subscription Day Offset",	{ []() {
			Json::Value data;
			data["day"] = (int)_round(g_subscriptionDay * 31.0f);
			data["month"] = (int)_round(g_subscriptionMonth * 13.0f);
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetSubscriptionDayOffset, data);
		} } },
		{ ASEO_subscriptionDayOffset,		"Subscription Day Offset",		{ g_subscriptionDay,  0, 31, []() {
		} } },
		{ ASEO_subscriptionMonthOffset,		"Subscription Month Offset",	{ g_subscriptionMonth,  0, 13, []() {
		} } },
		{ ASEO_resetSubscriptionVip,		"Reset Subscription Vip",	{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetSubscriptionVip, data);
		} } },
		{ ASEO_randomizeOutposts,			"Randomize Outposts",			{ []() {
			Json::Value data;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::RandomOutposts, data);
		} } },
		//dat.bn: Trigger ad reward receiving
		{ ASEO_receiveAdsReward,			"Receive Ads Reward",			{ []() {
			Json::Value data;
			data["name"] = "shop_daily";
			data["qty"] = 1;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendAdsReward(StringHash("shop_daily"), "shop_daily", 1);
		} } },

		{ ASEO_resetShopDailyAdsCount,			"Reset Shop Daily Ads Reward",			{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetShopDailyAdsCount);
		} } },


		//{ ASEO_geolocation_100,			"Geolocation 100",					{ GetSettingsMgr()->m_geolocation_100,	[]() {
		//	GetSettingsMgr()->m_geolocation_1000 = false;
		//	GetSettingsMgr()->m_geolocation_5000 = false;
		//	GetOnlineManager()->GetAHGSLobbyManager()->ClientSendGeoLocation();
		//} } },
		//{ ASEO_geolocation_1000,		"Geolocation 1000",					{ GetSettingsMgr()->m_geolocation_1000,	[]() {
		//	GetSettingsMgr()->m_geolocation_100 = false;
		//	GetSettingsMgr()->m_geolocation_5000 = false;
		//	GetOnlineManager()->GetAHGSLobbyManager()->ClientSendGeoLocation();
		//} } },
		//{ ASEO_geolocation_5000,		"Geolocation 5000",					{ GetSettingsMgr()->m_geolocation_5000,	[]() {
		//	GetSettingsMgr()->m_geolocation_100 = false;
		//	GetSettingsMgr()->m_geolocation_1000 = false;
		//	GetOnlineManager()->GetAHGSLobbyManager()->ClientSendGeoLocation();
		//} } },
		}, {}, }, // --- end profile subcategory

		///////////////////////////////////////////////////////
			{ "clan", "Clan", 0, {

		{ ASEO_league_up,				"ELO up",							{ []() {
			Json::Value data;
			data["direction"] = "up";
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ChangeMyElo, data);
		} } },
		{ ASEO_league_down,				"ELO down",							{ []() {
			Json::Value data;
			data["direction"] = "down";
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ChangeMyElo, data);
		} } },
		{ ASEO_clan_rating_up,			"Clan rating +100",					{ []() {
			Json::Value data;
			data["value"] = 100;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ClanRating, data);
		} } },
		{ ASEO_clan_rating_down,		"Clan rating -100",					{ []() {
			Json::Value data;
			data["value"] = -100;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ClanRating, data);
		} } },
		{ ASEO_AdvanceClanWarTime,		"Advance clan war time",			{ []() {
			if (GetClansManager()->MyClan())
				GetClansManager()->Cheat_AdvanceClanWarTime();
		} } },
		{ ASEO_UpgradeClanBases,		"Upgrade rand clan buliding",		{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::UpgradeClanBases);
		} } },
		{ ASEO_clan_war_cash,			"WarCash +10",						{ []() {
			Json::Value data;
			data["value"] = 10;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ClanWarCash, data);
		} } },
		{ ASEO_more_clan_war_cash,		"WarCash +100",						{ []() {
			Json::Value data;
			data["value"] = 100;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ClanWarCash, data);
		} } },
		{ ASEO_clan_rookie_status,		"Toggle rookie",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ToggleJoinedAtField);
		} } },
		{ ASEO_clan_battlepass_give_xp,	"Give 2455 clan battlepass XP",		{ []() {
			Json::Value data;
			data["currency"] = "clan_battlepass_xp";
			data["value"] = 2455;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::ResetCurrency, data);
		} } },
		{ ASEO_clan_skip_leave_cooldown,		"Skip Leave Clan Cooldown",					{ []() {
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SkipClanLeaveCooldown);
		} } },
	}, {}, }, // --- end clan subcategory

		///////////////////////////////////////////////////////
			{ "droneOps", "DroneOps", 0, {

		{ ASEO_DroneOpsChapter,			"Chapter",							{ CMissionComponent::m_fCurrentDroneTestChapterScroll,  1, MissionDataManager()->DroneOpsChapterCount(), []() {
		} } },
		{ ASEO_DroneOpsMissionIndex,	"Mission",							{ CMissionComponent::m_fCurrentDroneTestMissionScroll,  1, 25, []() {
		} } },
		{ ASEO_DroneOpsMissionWinding,	"Clockwise",						CMissionComponent::m_bCurrentDroneDirectionSwitch },
		{ ASEO_DroneOpsMissionStart,	"Start",							{ []() {
			auto mdmgr = MissionDataManager();

			CMissionComponent::m_nCurrentDroneTestChapter =
				(int)_round(CMissionComponent::m_fCurrentDroneTestChapterScroll * (mdmgr->DroneOpsChapterCount() - 1));

			CMissionComponent::m_nCurrentDroneTestMission =
				(int)_round(CMissionComponent::m_fCurrentDroneTestMissionScroll * 24);

			CMissionComponent::m_nCurrentDroneDirection = CMissionComponent::m_bCurrentDroneDirectionSwitch ? 1 : 2;

			auto mci = mdmgr->DronesChapterToMissionsChapter(CMissionComponent::m_nCurrentDroneTestChapter);
			MissionUid muid(mci, mdmgr->GetBlockTypeFromName("EndlessScoreAttack"), 0);

			Map_startMission(muid);
		} } },
	}, {}, }, // --- end droneops subcategory

		///////////////////////////////////////////////////////
		{ "BattleRoyale", "BR", DEBUG_DATA_CHEATS_BR, {

		{ ASEO_BR_StartGame,	"Start level",				{ []() {
			Application::GetInstance()->RequestLoadArenaTutorial();
		} } },
		{ ASEO_MPautomatch,		"Automatch",				{ []() {
			GetMpManager()->Reset();
			Json::Value data;
			data["automatch"] = true;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatMP, data);
		} } },
		{ ASEO_MPGetAllMods,	"GetAllMods",			{ []() {
			GetMpManager()->Reset();
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::FreeMods);
		} } },
		{ ASEO_MPincRating,		"Rating +50",				{ []() {
			static int delta = 50;
			Json::Value data;
			data["rating"] = delta;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatMP, data);
		} } },
		{ ASEO_MPdecRating,		"Rating -50",				{ []() {
			static int delta = -50;
			Json::Value data;
			data["rating"] = delta;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatMP, data);
		} } },
		{ ASEO_MPMarkAllTargets,"IdentifyAllPlayers",		k_markAllBRTargets },
		{ ASEO_MPKillAllBots,	"KillAllBots",				{ []() {
			GetMpManager()->SendCheatCommand(CheatMsg::KillAllBots);
		} } },
		{ ASEO_MP_MatchMake,	"MatchMake",				{ []() {
			GetMpManager()->Reset();
			Json::Value data;
			data["matchmake"] = true;
			GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::CheatMP, data);
		} } },
}, {}, }, // --- end BR subcategory

	///////////////////////////////////////////////////////

	{ "defenderLB", "DefLB", 0, {

			{ ASEO_setUndefeatedStreak,	"Set Undefeated Streak Days",	{ []() {
				Json::Value data;
				data["day"] = (int)_round(g_undefeatedStreak * 31.0f);
				GetOnlineManager()->GetAHGSLobbyManager()->ClientSendCheatCommand(AHCheatCommand::SetUndefeatedStreak, data);
			} } },
			{ ASEO_undefeatedStreakDays,		"Undefeated Streak Days",		{ g_undefeatedStreak,  0, 31, []() {
			} } },
	}, {}, }, // --- end Defender Leaderboard subcategory

	///////////////////////////////////////////////////////

			{ "mission", "Mission", DEBUG_DATA_CHEATS_ENDLEVEL, {

		{ ASEO_god,						"God Mode",							GetSettingsMgr()->m_godMode },
		{ ASEO_infiniteAmmo,			"Infinite Ammo",					GetSettingsMgr()->m_infiniteAmmo },
		{ ASEO_slowTime,				"Infinite & continuous Slow Time",	GetSettingsMgr()->m_bCheatSlowTime },
		{ ASEO_infiniteThermal,			"Infinite Thermal",					CLevel::s_infiniteThermal },
		{ ASEO_peInfo,					"Post Effects info",				PostEffects::s_DebugStatus },
		{ ASEO_showPlayerPosition,		"Show Player Position",				CLevel::s_bDebugPlayerPosition },
		{ ASEO_showInactiveEnemies,		"Show inactive enemies",			CLevel::m_ShowInactiveEnemies },
		{ ASEO_showObstacles,			"Show Obstacles",					CLevel::s_bDebugCol },
		{ ASEO_showCollisions,			"Show Collisions",					CLevel::s_bDebugCol },
		{ ASEO_showCollisionBodies,		"Show Collision Bodies",			CLevel::s_bDebugColBodies },
		{ ASEO_showCovers,				"Show Covers",						CLevel::s_ShowCovers },
		{ ASEO_showTriggers,			"Show Triggers",					CLevel::s_bDebugTrigger },
		{ ASEO_debugCutscenes,			"Debug cutscenes",					CLevel::s_bDebugCutscenes },
		{ ASEO_renderHud,				"Render HUD",						{ Hud::s_bRenderHud, []() {
			GetMenuManager()->EnableMenuRendering(Hud::s_bRenderHud, k_Hud);
		} } },
		{ ASEO_designInfo,				"Design Info",						CLevel::s_bDesignInfo },
		{ ASEO_zoneInfo,				"Zone Info",						CLevel::s_bShowZoneInfo },
		{ ASEO_weaponInfo,				"Weapon Info",						CLevel::s_bShowWeaponInfo },
		{ ASEO_hudControls,				"HUD controls debug",				CLevel::s_bDebugHudControls },
		{ ASEO_freezeTimer,				"Ignore level fail",				CLevel::s_freezeTimer },
		{ ASEO_aim,						"Auto aim",							GetSettingsMgr()->m_autoAim },
		{ ASEO_autoShoot,				"Auto shoot",						GetSettingsMgr()->m_autoShoot },
		{ ASEO_linkedFire,				"Linked Fire",						GetSettingsMgr()->m_bEnableLinkedFire },
		{ ASEO_heatSeeker,				"Heat Seeker",						GetSettingsMgr()->m_bEnableHeatSeeker },
		{ ASEO_chargeUp,				"Charge Up",						GetSettingsMgr()->m_bEnableChargeUp },
		{ ASEO_magnifyHeads,			"Magnify Heads",					GetSettingsMgr()->m_bMagnifyHeads },
		{ ASEO_endLevel,				"Win Level",						{ []() {
			GetMenuManager()->SetMenuActive(false, k_Hud);
			GetMenuManager()->SetMenuActive(false, k_InGameMenu);

			GS_InGameMenu* pState = (GS_InGameMenu*)Application::GetInstance()->CurrentState();
			pState->m_exitState = k_EndLevel;
		} } },
		{ ASEO_loseLevel,				"Lose level",						{ []() {
			GetMenuManager()->SetMenuActive(false, k_Hud);
			GetMenuManager()->SetMenuActive(false, k_InGameMenu);

			GS_InGameMenu* pState = (GS_InGameMenu*)Application::GetInstance()->CurrentState();
			pState->m_exitState = k_LoseLevel;
		} } },
		{ ASEO_restartLevel,			"Restart level",					{ []() {
			GetHud()->RestartLevelCheat();
		} } },
		}, {}, }, // --- end mission subcategory
	} }, // --- end cheats category

		////////////////////////////////////////////////////////////////////////////////////////////////////
		{ "soundsDebug", "SOUNDS DEBUG", DEBUG_DATA_SOUNDS,
	{
		{ ASEO_debugSound,				"Info",							{ CLevel::s_bSoundDebugInfo, []() {
			if (!CLevel::s_bSoundDebugInfo)
			{
				GetVoxSoundManager()->SetSoundVolume(k_soundGroupSfx, GetSettingsMgr()->m_FxVolume);
				GetVoxSoundManager()->SetSoundVolume(k_soundGroupMusic, GetSettingsMgr()->m_MusicVolume);
			}
		} } },
		{ ASEO_debugChristmasSound,				"Christmas Sound",							{ CLevel::s_bChristmasSound, []() {
					
			if(GetMenuManager()->IsMenuActive(k_MainMenu))
			{ 
				GetVoxSoundManager()->PlayMenuMusic(true, 2000);
			}			
		} } },
	},
	{}, 
	}, // --- end soundsDebug category

	};

	static std::map<std::string, std::string> dcLabels;
	if (dcLabels.empty())
	{
		int aseo = ASEO_SwitchDCStart;
		auto& debugMenu = categories.front();	// --- debug menu
		
		auto dcs = GetGaia()->GetAvailableDataCenters();
		auto currentDC = GetGaia()->GetDataCenter();
		for (auto dc : dcs)
		{
			if (dc.GetName() != currentDC)
			{
				std::string label = "Set DataCenter 2 " + dc.GetID() + "(" + dc.GetName() + ")";
				dcLabels[dc.GetName()] = label;

				debugMenu.entries.push_back(
					{
					(k_asevent_option)aseo,
					dcLabels[dc.GetName()].c_str(),
					{ [dc]() {
						GetGaia()->SaveDataCenter(dc.GetName());
						GameLoginManager::SetDataCenterChangeRestartRequired(true);
					} } });
				if (++aseo > ASEO_SwitchDCEnd)
					break;
			}
		}
		if (!dcLabels.empty())
		{
			debugMenu.entries.push_back(
				{
				(k_asevent_option)aseo,
				"Reset DataCenter",
				{ []() {
				GetGaia()->ResetSavedDataCenter();
				GameLoginManager::SetDataCenterChangeRestartRequired(true);
			} } });
			++aseo;
		}
	}

#endif // defined(DISABLE_ALL_CHEATS)
	return categories;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

gameswf::ASArray* ConstructDebugList(gameswf::Player* swfplayer)
{
	auto debugList = swfnew gameswf::ASArray(swfplayer);

	for (const auto& cat : CheatCategories())
	{
		if (!cat.flags || (g_debugData & cat.flags) == 0)
			continue;

		auto entry = swfnew gameswf::ASObject(swfplayer);
		entry->setMemberByName("id", cat.id);
		entry->setMemberByName("name", cat.label);
		debugList->push(entry);
	}

	return debugList;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void onAskForDebugData(const gameswf::ASNativeEventState& eventState)
{
	auto catId = StringHash(eventState.m_event["data"]["id"].toCStr());

	auto data = swfnew gameswf::ASObject(eventState.m_fx->getPlayer());

	auto addEntries = [](gameswf::ASObject* data, const std::vector<CheatEntry>& entries)
	{
		if (entries.empty())
			return;

		auto controlsList = swfnew gameswf::ASArray(data->getPlayer());
		data->setMemberByName("controlsList", controlsList);

		for (const auto& entry : entries)
		{
			controlsList->push(entry.create(data->getPlayer()));
		}
	};

	for (const auto& cat : CheatCategories())
	{
		if (catId != StringHash(cat.id))
			continue;
		if (!cat.flags || (g_debugData & cat.flags) == 0)
			continue;

		addEntries(data, cat.entries);

		if (cat.subcategories.empty())
			continue;

		auto subcatList = swfnew gameswf::ASArray(data->getPlayer());
		data->setMemberByName("cheatsList", subcatList);

		for (const auto& cat : cat.subcategories)
		{
			if (cat.flags && (g_debugData & cat.flags) == 0)
				continue;

			if ((cat.flags & DEBUG_DATA_CHEATS_ENDLEVEL) != 0 &&
				(!GetLevel() || GetLevel()->IsLevelMenu()))
				continue;

			auto subcat = swfnew gameswf::ASObject(data->getPlayer());
			subcatList->push(subcat);

			subcat->setMember("name", cat.label);

			addEntries(subcat, cat.entries);
		}
	}

#ifdef DEBUG_SOUND_EMITTERS
	auto& categories = CheatCategories();
	auto cit = std::find_if(categories.begin(), categories.end(), [](const CheatCategory& cat)
	{
		return StringHash(cat.id) == "soundsDebug";
	});
	if (cit != categories.end() && catId == "soundsDebug")
	{
		// --- append dynamically generated controls for each sound group
		std::vector<core::stringc> names;
		GetVoxSoundManager()->GetGroupNames(names);

		auto soundEntriesObj = gameswf::ASValue(data)["controlsList"].toObject();
		GLF_ASSERT(soundEntriesObj->is(gameswf::AS_ARRAY));
		auto soundEntries = static_cast<gameswf::ASArray*>(soundEntriesObj);

		for (int i = 0; i < names.size(); i++)
		{
			APPEND_TYPED_ASOBJECT(soundEntries, names[i].c_str(), ASEO_debugSoundGroup + i, GetVoxSoundManager()->GetGroupVolume(i), "scrollBar")
		}
	}
#endif // DEBUG_SOUND_EMITTERS

	GET_BASE_RETURN(data);
	eventState.m_fx->getStage().dispatchEvent(CPP_SEND_DEBUG_MENU_DATA, members, membersLength);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Cheats_HandleCheatOption(int option, const gameswf::ASValue& optionValue)
{
	auto optionId = (k_asevent_option)option;

	for (const auto& cat : CheatCategories())
	{
		const CheatEntry* entry = nullptr;
		auto it = std::find(cat.entries.begin(), cat.entries.end(), optionId);
		if (it == cat.entries.end())
		{
			for (const auto& scat : cat.subcategories)
			{
				auto scit = std::find(scat.entries.begin(), scat.entries.end(), optionId);
				if (scit == scat.entries.end())
					continue;
				entry = &(*scit);
			}
		}
		else
		{
			entry = &(*it);
		}
		if (!entry)
			continue;

		entry->handle(optionValue);

		return;
	}

	// --- dynamically generated controls
	if (optionId >= ASEO_debugSoundGroup && optionId <= ASEO_debugSoundGroupEnd && CLevel::s_bSoundDebugInfo)
	{
		GetVoxSoundManager()->SetVolumeByGroupId(optionId - ASEO_debugSoundGroup, optionValue.toFloat());
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
