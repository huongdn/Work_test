//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __GAMESERVER_FED_MANAGER_H__
#define __GAMESERVER_FED_MANAGER_H__

#include "json/json.h"
#include "Hash.h"
#include "FakeSmallMap.h"
#include "Missions/MissionUid.h"
#include "Internals/Utils/rng_wrap.h"
#include "Utils/EnumFlags.h"
#include "EloSettings.h"

#include <map>
#include <string>
#include <ctime>
#include <chrono>
#include <memory>
#include <random>
#include <unordered_map>
#include <set>

#define ADMIN_CLAN_TIMEOUT							300

#define DEFAULT_CLAN								("default")
#define DEFAULT_CLAN_MEMBER_LIMIT					(20)
#define MAX_CLAN_MEMBER_LIMIT						(20)

#define TRAINER_CLAN								("trainer")
#define TRAINER_CLAN_MEMBER_LIMIT					(4)

#define RED_BOSS_CLAN_LEADERBOARD
#define SKIRMISH_EVENT								// aka Arena TLE

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EventMessageCommand : int;
enum class E_OpponentType : int;
enum class E_FindPlayersReason : int;
enum class E_SurvivalLeaderboardType :int;
class SemEvent;
class ServerEvent;
class MpCrypto;
template <class T> struct TStatistics;

namespace antihackgs
{
	enum class E_ProfileOperation : unsigned char;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <comms/enums.h>

namespace comms
{
	class manager;
	class connection;
	class read_message;
}

namespace mpserver
{
	enum class MPAHMessageId : int;
	enum class RequestDisconnectReason : int8_t;
}

namespace mmserver
{
enum class MMAHMessageId : int;
enum class RequestDisconnectReason : int8_t;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace antihackgs
{

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CRequestsManager;
struct SRequest;

class FedComms;
class GenericDataPacket;
class NetworkMessageR;
class GSClient;

class AHMissionsData;
class AHEventsData;
class AHDroneOpsData;
class AHWeaponsData;
class PvpData;
class ServerSettings;
class NetworkMessageW;
class MultiplayerCommonData;
class GachaData;
class HestiaConfigManager;
class FakeNames;
class CollectionCodexData;

class SeshatProfileManager;
struct ProfileOperation;
struct LadderOperation;
struct ClanOperation;
struct ClanData;
struct ClanMember;

class ClanServerManager;

enum class ClanCommand : unsigned char;
enum class BattlePassCommand : unsigned char;
enum class ClanCreateUpdateStatus : int;
enum class ShopCommand : unsigned char;
enum class SquadOpsCommand : unsigned char;
enum class AutomatchCommand : unsigned char;
enum class ConquestCommand : unsigned char;

enum class ProcessClanType
{
	Default,
	Opponent,
	Other,
};

enum class ChatType
{
	Clan,
	Global
};

namespace tracking
{
	struct TrackingRequest;
}

class BossEventData;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern std::string g_filePath;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::map<std::string, GSClient*>::iterator GSClientsIterator;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CachedClan
{
	int	trophyLimit;
	//int score;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MissionDropChancesData
{
	const Json::Value* missionDropChances = nullptr;
	const Json::Value* coopMissionDropChances = nullptr;
	const Json::Value* collectorDrops = nullptr;
	double collectorFactor = 0;
	const Json::Value* guaranteedDrops = nullptr;
	const Json::Value* bountyMissionDrops = nullptr;
	Json::Value extraData;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_DECLARE_ENUM_FLAGS(BountyChapterOp)
	EnsureSquadOps				= 0x0001,
	GenerateRandomPuzzle		= 0x0002,
	GenerateRandomMissions		= 0x0004,
	GenerateRandomBoss			= 0x0008,
	UsePredefinedMissions		= 0x0010,
	ComputeBossRewards			= 0x0020,

	FirstTimeChapterUnlock		= EnsureSquadOps | GenerateRandomPuzzle | UsePredefinedMissions | ComputeBossRewards,
END_DECLARE_ENUM_FLAGS(BountyChapterOp);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_DECLARE_ENUM_FLAGS(ModifierType)
	Weapon						= 0x0001,
	Avatar						= 0x0002,
	Clan						= 0x0004,
	ModifierForCredit			= 0x0008,
	Trainer						= 0x0010,
	BountyAvatar				= 0x0020,
	ArenaMods					= 0x0040,
	InAsyncPvP					= 0x0080,
	SubscriptionVip				= 0x0100,
	SubscriptionVipArena		= 0x0200,

	_Default					= Weapon | Avatar | Clan | ModifierForCredit | Trainer | SubscriptionVip,
END_DECLARE_ENUM_FLAGS(ModifierType);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class E_LangJson : int
{
	CustomJson,
	LoadingHints,
	GachaChances,

	_All,
};

enum EcommChOp
{
	OP_SEND_SMS = 0,
	OP_SEND_CODE,
	OP_SEND_PREGS,

	OP_ALL,
};

struct charLessCmp
{
	bool operator ()(const char* p1, const char* p2) const
	{
		return strcmp(p1, p2) < 0;
	}
};

class GameServerFedManager
{
	GameServerFedManager(const GameServerFedManager&) = delete;
	GameServerFedManager(GameServerFedManager&&) = delete;

public:

	GameServerFedManager();
	virtual ~GameServerFedManager();

	FedComms*				m_fedComms;

	//port for UNIX domain socket communication to lobby controller
	static char*			s_udsPortWithController;


	void					Update(const float fdt = 0.f);

	//////////////////////////////////////////////// INITIALIZE //////////////////////////////////////////////////////

	bool					InitializeGSManager(const std::string& pid);
	bool					InitServerCommunication();

	void					SetLogHeader(const std::string& header);

	/////////////////////////////////////////// RESOURCES MANAGEMENT /////////////////////////////////////////////////

	void					SetResourcesPath(const std::string& path);
	void					SetRevision(const std::string& rev) { m_revision = rev; }
	//tuan.transy - add to visualize server revision on etsv3
	std::string				GetRevision() { return m_revision; }
	//end

	const Json::Value&		GetCustomJson(const GSClient* client) const;
	const Json::Value&		GetCustomJson(const GSClient* client, const std::string& label) const;
	Json::Value				GetLoadingHints(GSClient* client) const;
	Json::Value				GetChancesJson(GSClient* client) const;
	int						GetServerConfigParam(const std::string& label) const;

	rng_wrap				m_rng;	// --- global random generator for less important stuff

	virtual void			ClaimDailyReward(GSClient *client, short day) = 0;
	virtual void			ClaimDailyRewardV2(GSClient *client, short day, bool isCheat = false) = 0;
	virtual void			ClaimSevenDaysReward(GSClient* client, short day, bool isSpecial = false) = 0;

protected:
	const std::string&		GetResourcesPath() { return g_filePath; }	

	virtual bool			LoadLocalJsonFiles();
	bool					LoadInitialProfile();
	bool					LoadMissionsData();
	bool					LoadCoopEventsData();
	bool					LoadDroneOpsData();
	bool					LoadWeaponsData();
	bool					LoadServerSettings();
	bool					LoadMultiplayerData();
	bool					LoadPvpData();
	bool					LoadDailyData();
	bool					LoadAchievementsData();
	bool					LoadLPNData();
	bool					LoadRandomNames();
	bool					LoadLeaderboardExpiry();
#ifdef SKIRMISH_EVENT
	bool					LoadSkirmishSchedule();
#endif // SKIRMISH_EVENT
	bool					LoadCustomDataJson();
	bool					LoadLoadingHintsJson();
	bool					LoadChancesJson();
	bool					LoadServerConfigParamsJson();
	bool					LoadSplashScreensJson();
	bool					LoadMotdData();
	bool					LoadBaseShopData();
	bool					LoadGachaData();
	bool					LoadUuids();
	bool					LoadRequiredDlcVersion();
	bool					LoadCheatersList();
	bool					LoadCheatersList2();
	bool					LoadCollectionCodexData();

public:
	int						IsCheater(GSClient* client);

private:
	using charPSetT = std::set<const char*, charLessCmp>;
	bool					LoadCheatersList(const char* filename, char* &buff, charPSetT* &cheaters);
	std::vector<SemEvent*>	FilterCustomSemEvent(GSClient* client, std::string eventType);
	void					RegisterAllSemEventsV2();

// --- serverfiles
private:
	struct ServerFile
	{
		size_t uncompressedSize;
		std::string data;
		hash_t hash;
	};
	mcfwutil::fsmap<hash_t, ServerFile> m_serverFiles;

protected:
	bool ServerSendServerFileToClient(const std::string &credential, hash_t serverFileId, hash_t clientDataHash);

public:
	void AddServerFile(const char* fileId, const std::string& data);

private:
	void					GiveGiftReward(const std::string& credential, const Json::Value& gift, const std::string& gift_type);
	void					GiveFriendGiftReward(const std::string& credential, const int giftQty, const std::string& gift_type);;

	//////////////////////////////////////////////// Hestia internal stuff ///////////////////////////////////////////
private:
	enum E_HESTIA_REQUEST_TYPE
	{
		E_HRT_INITIAL,
		E_HRT_REFRESH,
	};

	enum E_SESHAT_SET_PROFILE_REQUEST_FLAGS
	{
		E_SSPRF_SEND_CONFIRMATION = 1 << 0,
		E_SSPRF_REQUEST_CONFIG = 1 << 1
	};

public:

	bool					ServerSendLevelFileToClient(const std::string &credential, bool fileFound, const char* data = NULL, const int size = 0);
	bool					ServerSendArmoryFileToClient(const std::string &credential, const char* data = NULL, const int size = 0);

	bool					ServerSendPvpTrackingDataToClient(const GSClient* client, const Json::Value& jEventData);
	bool					ServerSendEventsData(const GSClient* client, const Json::Value &jResponse);
	bool					ServerSendEventUpdate(const GSClient* client, const Json::Value &jScores);
	bool					ServerSendEventUpdate(const GSClient* client, const std::string &eventID, bool inhibitClientRefresh = false);
	bool					ServerRespondEventsLadder(const GSClient* client, const char* leaderboard);
	bool					ServerSendChatMessage(const GSClient* client, const std::string& message, int room, bool freeToSend);
	bool					ServerSendCollectionCodexData(const GSClient* client, const Json::Value& jCodexData, const std::string& field = "all");
	bool					ServerSendColletionItemSource(const GSClient* client, const Json::Value& jData);

	void					UpdateEventScoresFromStatistics(GSClient* client, const Json::Value& jStatistics, hash_t weapon, const std::string& filterEventID, const MissionUid &muid);
	void					ClientParticipateEvents(GSClient* client, const Json::Value& eventIDs);
	void					ClientCheckMultipleStageTleEventWeapon(GSClient* client, const Json::Value& eventIDs);
	bool					ServerSendEloLadder(const GSClient* client, StringHash ladderType, const char* ladder);

	bool					ServerSendDailyQuestsPatch(GSClient* client, const Json::Value& data);

	/////////////////////////////////////////// RECEIVE & HANDLE DATA ////////////////////////////////////////////////

	//geo - for now we will handle messages on the spot - no need to save them in another queue. 
	void					ReceiveAndHandleDataFromClients();

	int						ParseAndHandleMessages(unsigned char* data, unsigned int dataSize, const std::string &credential);
	virtual void			HandleTCPMessage(unsigned char* data, unsigned int dataSize, const std::string &credential);
	void					HandleBaseMessages(unsigned char messageType, unsigned int messageSize, NetworkMessageR &recvData, const std::string &credential);
	int						KeepLeftOversInRecvBuff(unsigned char* dstBuff, unsigned char* srcBuff, unsigned int &size);

	void					UpdateFrameTime();
	time_t					GetServerTime() const { return m_serverTime; }

	int64_t					m_lastTimeSentKeepAlive;
	void					UpdateConnectionMonitor();
	void					SendPingsToAllClients();
	bool					ServerSendPingToClient(GSClient* client, bool withTs);
	void					CheckAndHandIdlePlayers();
	bool					ServerSendConnectionOK(const std::string& credential);
	void					UpdateSafeRemoveClients();
	void					UpdateTimedSave();
	void					UpdateTimedEventRefresh();
	void					UpdateLanguageChange();
	void					UpdateSpecialAdsCooldownTime();
	void					UpdateSpinWheelCooldownTime(GSClient* client, int spin_type);
	void					SetFreeSpinCheatTimer(GSClient* client, int freeSpinType);
	int						GetSpecialAdsType(bool isBattlepacks, int cooldownValue, int spin_free_type);
	void					UpdateHestiaConfigs();
	void					SendServerTimeUpdates();
	void					UpdateSquadmatesStatus();
	void					UpdateItemsExpiry();
	void					UpdatePendingAutoassemble();
	void					UpdateClans();
	void					UpdateTrainerClans();
	void					UpdateSquadmatesCheat();
	void					UpdateAntiExploitationSystemExpiry();

	int64_t					GetCheckIncomingAttacksInterval() const;

	void					CheckSevenDays(GSClient* client, std::string eventKey, bool bCheat = false);
	void					CheckDailyLogin(GSClient *client, bool bCheat = false);
	void					CheckDailyLoginV2(GSClient *client, bool bCheat = false);
	void					UpdatePiggyBankTier(GSClient *client);
	void					SentPiggyBankConfig(GSClient *client);
	void					GiveComebackReward(GSClient* client);
	void					GiveUpdateNewVersionReward(GSClient* client);
	void					CheckComebackRewardTimestamp(const GSClient* client);
	void					AddAchievementProgress(const GSClient* client, const Json::Value& jStatistics);
	double					GetAchievementProgress(const GSClient* client, hash_t type);
	void					AddAchievementProgress(const GSClient* client, hash_t type, double addProgress);
	bool					SetAchievementProgress(const GSClient* client, hash_t type, double addProgress);
	bool					SetUnconditionalAchievementProgress(const GSClient* client, hash_t type, double addProgress);	
	void					UnlockAllAchievements(const GSClient* client);
	void					UnlockRandomAchievement(const GSClient* client);
	void					AccelerateAchievements(const GSClient* client);
	void					ServerSendAchievementProgress(const GSClient* client, const Json::Value &jUpdate);
	void					ClaimAchievementReward(GSClient *client, int type, int tier);
	bool					IsAchievementCompleted(GSClient *client, int type, int tier);
	void					HandleEventCommand(GSClient* client, EventMessageCommand command, const std::string& eventId, int missionIndex, int chapterIndex = -1);
	virtual bool			ProcessOsirisInfo(GSClient* client, std::string& jsonStr) = 0;

	bool					SendGachaInfoToClient(GSClient* client, StringHash pack, const void* ret);
	virtual bool			ServerSendUpdateDefenderLeaderboardStats(GSClient* client) = 0;

	//////////////////////////////////////////// MMSERVER /////////////////////////////////////////////////
private:
	struct MMServerConnection
	{
	private:
		comms::connection*	_connection = nullptr;
	public:
		std::vector<std::string> pendingClients;	// credential
		std::vector<std::string> public_ips;
		bool bConnInProgress = false;
		bool bConnDone = false;
		bool isDone() { return _connection && bConnDone; }
		bool isInProgress() { return bConnInProgress; }
		void setInProgress() { bConnInProgress = true; }
		void setDone() { bConnDone = true; }
		comms::connection* connection() { return _connection; }
		void connection(comms::connection* c) { _connection = c; bConnDone = bConnInProgress = false; }
	} m_MMServerConnection;

	void					UpdateMMServer();
public:

	void					ConnectToMMServer(GSClient* client, bool allow_local = true);
	void					HandleMMDisconnect(GSClient* client, mmserver::RequestDisconnectReason reason, int error = 0);
	void					HandleMMDisconnect(comms::connection q, comms::Connection_error reason, int error = 0);
	void					HandleMMDisconnect(comms::Connection_error reason, int error = 0);
	static void				callbackConnectToMMServer(const SRequest& requestData);
	static void				callbackMMServerToken(const SRequest& requestData);

	//////////////////////////////////////////// SCRIPTS MANAGEMENT /////////////////////////////////////////////////

	void					UpdateScripts(const float fdt);

	void					ScriptRequestInitialData(const GSClient* client);
	static void				callbackScriptInitialData(const SRequest& requestData);

	void					ScriptRequestFedId(GSClient* client);
	static void				callbackScriptRequestFedId(const SRequest& requestData);

	void					ScriptRequestSeshatProfile(GSClient* client);
	static void				callbackScriptGetSeshatProfile(const SRequest& requestData);

	void					ScriptRequestSeshatProfileForOther(GSClient* client, const std::string &credential, E_ProfileOperation reason, std::string custom_data = "");
	static void				callbackScriptGetSeshatProfileForOther(const SRequest& requestData);

	void					ScriptRequestSeshatProfileForOtherForDisplay(GSClient* client, const std::string &credential);
	static void				callbackScriptGetSeshatProfileForOtherForDisplay(const SRequest& requestData);

	void					ScriptRequestSeshatECommInventory(GSClient* client, hash_t transactionId, bool suppress_no_change_error);
	static void				callbackScriptGetSeshatECommInventory(const SRequest& requestData);

	void					ScriptRequestSeshatProfileField(const std::string& fields, std::unique_ptr<ProfileOperation> operation);
	static void				callbackScriptGetSeshatProfileField(const SRequest& requestData);

	void					ScriptRequestSeshatProfileFieldWithEtag(const std::string& fields, std::unique_ptr<ProfileOperation> operation, const std::string& etag_for = "");
	static void				callbackScriptGetSeshatProfileFieldWithEtag(const SRequest& requestData);

	void					ScriptSetSeshatProfileField(const Json::Value& obj, std::unique_ptr<ProfileOperation> operation);
	static void				callbackScriptSetSeshatProfileField(const SRequest& requestData);

	void					ScriptChangeSeshatProfileField(std::string obj, const std::string& selector, const std::string& seshat_operation, std::unique_ptr<ProfileOperation> operation);
	static void				callbackScriptChangeSeshatProfileField(const SRequest& requestData);

	void					ScriptChangeSeshatProfileFieldWithEtag(const char* obj, const std::string& selector, const std::string& seshat_operation, const std::string& etag, std::unique_ptr<ProfileOperation> operation);
	static void				callbackScriptChangeSeshatProfileFieldWithEtag(const SRequest& requestData);

	void					ScriptRequestHestiaConfig(GSClient* client, E_HESTIA_REQUEST_TYPE requestType);
	static void				callbackScriptGetHestiaConfig(const SRequest& requestData);

	void					ScriptSetSeshatProfile(const std::string &credential, GSClient* client, unsigned int requestFlags = 0);
	static void				callbackScriptSetSeshatProfile(const SRequest& requestData);

	void					ScriptRequestRemoveKeysFromProfile(GSClient* client);
	static void				callbackScriptRequestRemoveKeysFromProfile(const SRequest& requestData);

	void					ScriptRequestTrackOfflinePurchase(GSClient* client, const Json::Value& purchase);
	static void				callbackScriptRequestTrackOfflinePurchase(const SRequest& requestData);

	void					ScriptRequestFilterMessage(GSClient* client, std::string message, const char* language, int chat);
	static void				callbackScriptRequestFilterMessage(const SRequest& requestData);

	void					ScriptRequestGiftMessage(const std::string& credential, const std::string& msgID);
	static void				callbackScriptRequestGiftMessage(const SRequest& requestData);
	void					ScriptRequestDeleteGiftMessage(const std::string& credential, const std::string& msgID);
	static void				callbackScriptRequestDeleteGiftMessage(const SRequest& requestData);
	void					ScriptSendHermesNotification(const std::string& clientId, const std::vector<std::string>& fedIds, const std::string& category, const std::string& info, unsigned int key, unsigned char mask, std::unique_ptr<ClanOperation> op = nullptr);
	static void				callbackScriptSendHermesNotification(const SRequest& requestData);

	void					ScriptSendHermesMessageToUsers(const std::string& clientId, const std::vector<ClanMember>& recipients, const Json::Value jData);
	static void				callbackScriptSendHermesMessageToUsers(const SRequest& requestData);

	void					ScriptRequestEventsInfo(GSClient *client);
	static void				callbackScriptRequestEventsInfo(const SRequest &_requestData);
	static void				PatchEventVersion(Json::Value& templateData); //To handle add "_event_version" field in "event_tuning" for SemEvent V2

	void					ScriptPostEventRankedScore(const GSClient *client, const Json::Value &jScores, bool autoRefreshLadder = false);
	static void				callbackScriptPostEventRankedScore(const SRequest &_requestData);
	void					ScriptRequestEventsLadder(const GSClient *client, Json::Value &jEventIDs);
	static void				callbackScriptRequestEventsLadder(const SRequest &_requestData);

	enum EMatchmakingOperation { E_FindOpponents, E_FindPlayers };
	using OverrideT = std::vector<std::pair<std::string, std::string>>;
	void					ScriptRequestGetMatches(const GSClient* client, std::string name, int limit, const std::string& include, const OverrideT& ovr, Json::Value customData, bool location, const std::string& etag_for, std::unique_ptr<ProfileOperation> operation);
	static void				callbackScriptGetMatches(const SRequest& requestData);

	void					ScriptSetOtherSeshatProfile(const std::string &credential, const std::string& object, const Json::Value& extraData);
	static void				callbackScriptSetOtherSeshatProfile(const SRequest& requestData);

	void					ScriptDeleteSeshatProfileSelector(const GSClient* client, const std::string &credential, const std::string& selector, const Json::Value& extraData);
	static void				callbackScriptDeleteSeshatProfileSelector(const SRequest& requestData);

	void					ScriptRequestListFriends(const GSClient* client);
	static void				callbackScriptListFriends(const SRequest& requestData);

	void					ScriptRequestUpdateStatusLine(const GSClient* client, const std::string& statusLine);
	static void				callbackScriptUpdateStatusLine(const SRequest& requestData);

	void					ScriptRequestGetOsirisProfile(const GSClient* client, bool initialRequest = false);
	static void				callbackScriptGetOsirisProfile(const SRequest& requestData);

	void					ScriptRequestUpdateOsirisProfile(const GSClient* client, const std::string& name, const std::string& country, const std::string& language);
	static void				callbackScriptUpdateOsirisProfile(const SRequest& requestData);
	
	void					ScriptRequestGetCredentialFromAlias(const GSClient* client, const std::string& alias);
	static void				callbackScriptGetCredentialFromAlias(const SRequest& requestData);
	
	void					ScriptRequestAddConnection(const GSClient* client, const std::string& target_credential, const std::string& target_alias);
	static void				callbackScriptAddConnection(const SRequest& requestData);

	void					ScriptGetEloLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptGetEloLadder(const SRequest& requestData);

#ifdef RED_BOSS_CLAN_LEADERBOARD
	void					ScriptPostRedBossClanLadder(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptPostRedBossClanLadder(const SRequest& requestData);
	void					ScriptGetRedBossClansLeaderboard(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptGetRedBossClansLeaderboard(const SRequest& requestData);
#endif // RED_BOSS_CLAN_LEADERBOARD

	void					ScriptPostRedBossLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptPostRedBossLadder(const SRequest& requestData);

	void					ScriptPostEloLadder(std::unique_ptr<LadderOperation> operation);
	void					ScriptPostEloTopPlayersLadder(std::unique_ptr<LadderOperation> operation, std::map<std::string, std::string>&& customParams);
	static void				callbackScriptPostEloLadder(const SRequest& requestData);

	void					ScriptDeleteFromLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptDeleteFromEloLadder(const SRequest& requestData);

	void					ScriptGetMPLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptGetMPLadder(const SRequest& requestData);

	void					ScriptPostMPLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptPostMPLadder(const SRequest& requestData);

#ifdef SKIRMISH_EVENT
	void					ScriptPostSkirmishLadder(std::unique_ptr<LadderOperation> operation, const std::string& eventId);
	static void				callbackScriptPostSkirmishLadder(const SRequest& requestData);

	void					ScriptGetSkirmishLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptGetSkirmishLadder(const SRequest& requestData);

	void					ScriptPostSkirmishClanLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptPostSkirmishClanLadder(const SRequest& requestData);

	void					ScriptGetSkirmishClanLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptGetSkirmishClanLadder(const SRequest& requestData);

	void					ScriptPostSkirmishHofLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptPostSkirmishHofLadder(const SRequest& requestData);

	void					ScriptGetSkirmishHofLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptGetSkirmishHofLadder(const SRequest& requestData);
#endif // SKIRMISH_EVENT
	void					ScriptPostDefenderLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptPostDefenderLadder(const SRequest& requestData);

	void					ScriptGetDefenderLadder(std::unique_ptr<LadderOperation> operation);
	static void				callbackScriptGetDefenderLadder(const SRequest& requestData);
	void					ScriptGetAlias(const GSClient* client);
	static void				callbackScriptGetAlias(const SRequest& requestData);

	void					ScriptUseReferralCode(const GSClient* client, const char* code);
	static void				callbackScriptUserReferralCode(const SRequest& requestData);

	void					ScriptGetTrainerClanFromAlias(const GSClient* client, const char* code);
	static void				callbackScriptGetTrainerClanFromAlias(const SRequest& requestData);
	
	void					ScriptSendEcommChinaRegistrationID(const GSClient* client, std::string phoneNumber);
	static void				callbackScriptSendEcommChinaRegistrationID(const SRequest& requestData);

	void					ScriptSendEcommChinaValidateCode(const GSClient* client, std::string phoneNumber, std::string national_id, std::string name, std::string pin);
	static void				callbackScriptSendEcommChinaValidateCode(const SRequest& requestData);

	void					ScriptAutomatch(GSClient* client, bool force_anubis = false);
	static void				callbackScriptAutomatch(const SRequest& requestData);
	static void				ConnectClientToRoom(GameServerFedManager *serverMgr, GSClient *client, const Json::Value& anubisRoom, bool force_anubis = false);

	void					ScriptEncryptToken(const GSClient* client, std::string nonce, Json::Value room);
	static void				callbackScriptEncryptToken(const SRequest& requestData);

	void					ScriptSendUserPersonalInfoData(const GSClient* client, int ggi, const std::string& firstName, const std::string& lastName,
									  const std::string& email, const std::string& birthDate, const std::string& country);
	static void				callbackScriptSendUserPersonalInfoData(const SRequest& requestData);

	///////////////////////////////////////// MC5 //////////////////////////////////////////////

	void					ScriptLookupAliasMC5(const GSClient* client);
	static void				callbackScriptLookupAliasMC5(const SRequest& requestData);

	///////////////////////////////////////// CLANS ////////////////////////////////////////////

	void					ScriptClanCreate(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanCreate(const SRequest& requestData);

	void					ScriptTrainerClanCreate(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptTrainerClanCreate(const SRequest& requestData);

	void					ScriptClanUpdate(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanUpdate(const SRequest& requestData);

	void					ScriptClanPromote(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanPromote(const SRequest& requestData);

	void					ScriptClanGet(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanGet(const SRequest& requestData);

	void					ScriptTrainerClanGet(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptTrainerClanGet(const SRequest& requestData);

	void					ScriptClanSearch(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanSearch(const SRequest& requestData);

	void					ScriptClanFixFields(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptFixFields(const SRequest& requestData);

	void					ScriptClanSetMemberField(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanSetMemberField(const SRequest& requestData);

	void					ScriptClanIncEventScore(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanIncEventScore(const SRequest& requestData);

	void					ScriptClanIncWarCash(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanIncWarCash(const SRequest& requestData);

	void					ScriptClanInvite(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanInvite(const SRequest& requestData);

	void					ScriptTrainerClanInvite(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptTrainerClanInvite(const SRequest& requestData);

	void					ScriptClanKick(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanKick(const SRequest& requestData);

	void					ScriptTrainerClanKick(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptTrainerClanKick(const SRequest& requestData);

	void					ScriptClanGetRequests(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanGetRequests(const SRequest& requestData);

	void					ScriptClanGetSentRequests(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanGetSentRequests(const SRequest& requestData);

	//void					ScriptClanCancelSentRequest(std::unique_ptr<ClanOperation> operation);
	//static void				callbackScriptClanCancelSentRequest(const SRequest& requestData);

	void					ScriptClanAcceptRequest(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanAcceptRequest(const SRequest& requestData);

	void					ScriptClanRejectRequest(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanRejectRequest(const SRequest& requestData);

	void					ScriptClanIgnoreRequest(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanIgnoreRequest(const SRequest& requestData);

	void					ScriptClanPostWallUser(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanPostWallUser(const SRequest& requestData);

	void					ScriptClanPostWallGame(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanPostWallGame(const SRequest& requestData);

	void					ScriptClanDeleteWallPost(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanDeleteWallPost(const SRequest& requestData);

	void					ScriptClanViewWall(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanViewWall(const SRequest& requestData);

	void					ScriptClanViewWallLast(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanViewWallLast(const SRequest& requestData);

	void					ScriptClanParticipateEvent(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanParticipateEvent(const SRequest& requestData);

	void					ScriptClanSendHermesMessageToUser(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanSendHermesMessageToUser(const SRequest& requestData);

	void					ScriptClanDeleteHermesMessage(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanDeleteHermesMessage(const SRequest& requestData);

	void					ScriptClanGetField(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanGetField(const SRequest& requestData);

	void					ScriptClanSetField(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClanSetField(const SRequest& requestData);

	//void					ScriptIncCheckProfileCounter(std::unique_ptr<ClanOperation> operation);
	//static void				callbackScriptIncCheckProfileCounter(const SRequest& requestData);

	void					StartGetingEloLadders(GSClient* client);
	void					StartGetingMPLadders(GSClient* client, bool getScore = true, bool getWins = true);
	void					StartGetingOutpostsLadders(GSClient* client);
#ifdef SKIRMISH_EVENT
	void					StartGettingSkirmishLadders(GSClient* client);
#endif // SKIRMISH_EVENT

	void					SendEtsRequest(Json::Value& requestData);
	void					ScriptSendEtsRequest(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptSendEtsRequest(const SRequest& requestData);

	void					ScriptRequestCheckRedeemCode(const GSClient* client, const char* redeemCode);
	static void				callbackScriptRequestCheckRedeemCode(const SRequest& requestData);
	
	void					ScriptClansLeaderboard(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptClansLeaderboard(const SRequest& requestData);

	void					ScriptRedBossIncEventScore(const GSClient* client, const std::string& eventId, double score);
	static void				callbackScriptRedBossIncEventScore(const SRequest& requestData);

	void					ScriptGlobalGroupParticipateEvent(const GSClient* client, const std::string& eventId);
	static void				callbackScriptGlobalGroupParticipateEvent(const SRequest& requestData);

private:
	std::unique_ptr<ClanServerManager> m_clanServerManager;

public:
	ClanServerManager* GetClanServerManager() const { return m_clanServerManager.get(); }
	std::string GetUUID(const std::string& key) const { return m_uuids[key].asString(); }

	struct ClansLeaderboardData
	{
		enum
		{
			CLANS_LEADERBOARD_CACHE_TIME = 9 * 60, // --- 9 minutes
		};

		ClansLeaderboardData(GameServerFedManager& mgr) : mgr_(mgr), ts_(0), min_score_(0), updating_(false)
#ifdef RED_BOSS_CLAN_LEADERBOARD
			, isDemonClanData_(false)
#endif // RED_BOSS_CLAN_LEADERBOARD
		{ }

		void SetData(Json::Value&& jData);
		const Json::Value& Data() const { return data_; }
		bool IsOld() const { return GameServerFedManager::Instance()->GetLocalTime() - ts_ > CLANS_LEADERBOARD_CACHE_TIME; }
		int MinScore() const { return min_score_; }

		void SetTimeUpdateStarted(bool started) { updating_ = started; }
		bool TimeUpdateStarted() const { return updating_; }
#ifdef RED_BOSS_CLAN_LEADERBOARD
		void SetDemonClanData(bool isDemonData) { isDemonClanData_ = isDemonData; }
#endif // RED_BOSS_CLAN_LEADERBOARD
	private:
		GameServerFedManager& mgr_;
		Json::Value data_;
		time_t ts_;
		int min_score_;
		bool updating_;
#ifdef RED_BOSS_CLAN_LEADERBOARD
		bool isDemonClanData_;
#endif // RED_BOSS_CLAN_LEADERBOARD
	};
	friend struct ClansLeaderboardData;
	ClansLeaderboardData&	ClansLeaderboard() { return m_clansLeaderboard; }
	void					StartGettingClansLeaderboard(std::unique_ptr<ClanOperation> operation, int refreshRequiredScore);
	void					UpdateClansLeaderboard();

	ClansLeaderboardData&	ClansChampions() { return m_clansChampions; }
	bool					UpdateClansChampions();

#ifdef RED_BOSS_CLAN_LEADERBOARD
	ClansLeaderboardData&	ClansDemon() { return m_clansDemon; }
#endif // RED_BOSS_CLAN_LEADERBOARD

	int						CheckIncWarCashCounter(GSClient* client, int fixed_amount = 0);
	int						IncWarCashCounter(GSClient *client, int cheat_amount = 0);

	void					IncHappyHourCounter(double value);
	void					FlushHappyHourCounter();
	bool					IsHappyHourActive() const { return m_happyHourEnd && m_happyHourEnd > GetServerTime(); }
	void					AddHappyHourData(Json::Value* inventory_changes);
	void					AddClientHappyHourData(const GSClient* client, Json::Value* inventory_changes);
	void					AddClientRedBossScoreData(const GSClient* client, Json::Value* inventory_changes);
	double					GetRedBossScore(const std::string& eventID) const;

	void					CheatLeaveClan(GSClient* client);

	const std::string&		GetPid() const { return m_PID; }
	const std::string&		GetDatacenter() const { return m_datacenter; }

private:
	CRequestsManager*		m_RequestsManager;

	std::map<int, std::string>	m_scriptTaskNames;
	void					InitScriptTaskNames();
	void					InitAdminOsirisProfile(const std::string& clientId);
	static void				callbackInitAdminOsirisProfile(const SRequest& requestData);
	void					UpdateAdminOsirisProfile();
	static void				callbackUpdateAdminOsirisProfile(const SRequest& requestData);
	void					IncDecAdminClanField(const std::string& field, double value);
	static void				callbackIncDecAdminClanField(const SRequest& requestData);
	void					ResetAdminClanField(const std::string& field, double value);
	void					OnAdminOsirisProfileUpdated();
	void					OnActivateHappyHour();

	ClansLeaderboardData		m_clansLeaderboard;
	ClansLeaderboardData		m_clansChampions;
#ifdef RED_BOSS_CLAN_LEADERBOARD
	ClansLeaderboardData		m_clansDemon;
#endif // RED_BOSS_CLAN_LEADERBOARD

	time_t					m_serverTime = 0;
	std::string				m_PID;
	std::string				m_datacenter;

	Json::Value				m_adminClan;
	std::string				m_adminClanAccessToken;
	std::string				m_adminClanClientId;
	bool					m_adminClanRequestPending = false;
	time_t					m_adminClanExpiry = 0;
	time_t					m_happyHourEnd = 0;
	double					m_happyHourPendingCounter = 0;
	time_t					m_happyHourPendingTimer = 0;
	std::vector<time_t>		m_spinWheelCooldownCrtTimer;
	time_t					m_battlepackCooldownCrtTimer = 0;


protected:
	typedef std::map<std::string, GSClient*> ClientsListMap;

public:

	///////////////////////////////////////// PLAYERS PROFILE MANAGEMENT ////////////////////////////////////////////

	bool					ServerSendBannedMessage(const std::string& credential, const std::string &message);
	bool					ServerSendProfileToPlayer(const std::string& credential, const SeshatProfileManager* profileManager);
	bool					ServerSendDefenseLogsToPlayer(const std::string& credential, const SeshatProfileManager* profileManager);
	bool					ServerSendConfigToPlayer(GSClient* client);
	bool					ServerSendProfileSaved(const std::string& credential);
	bool					ServerSendSlotsConfigToPlayer(const std::string& credential);
	bool					ServerSendProfileForOtherPlayer(const GSClient* client, Json::Value&& profile);
	bool					ServerSendJsonToPlayer(GSClient* client, E_LangJson type);
	bool					ServerSendDismissInfo(GSClient* client, const std::vector<StringHash>& squadmates, Json::Value info);

	bool					ServerSendTimeToPlayer(GSClient* client);
	bool					ServerSendInventoryUpdate(GSClient* client, StringHash transactionId, Json::Value changes);		// --- makes a copy because it may change it
	bool					ServerSendFindPartsMission(GSClient* client, hash_t weaponId, hash_t partId);
	bool					ServerSendFoundPlayersToClient(GSClient* client, E_FindPlayersReason reason, Json::Value&& response);

	bool					ServerSendMessage(const std::string& credential, char* message, int messageLen);
	bool					ServerSendIncompatibleMessage(const std::string& credential);
	bool					ServerSendAddReferralEnd(const std::string& credential, int errorCode);
	bool					ServerSendBPRewardVideoEnable(const std::string& credential, int bpType);

	bool					ServerSendDeletableChapters(GSClient* client);

	bool					ProcessGenericHermesNotification(GSClient* client, const std::string& msgId, const std::string& attachment);

	bool					ServerSendNewEncryptionKeys(GSClient* client);
	bool					ServerSendOsirisInfo(GSClient* client, const std::string& osirisInfo);
	bool					ServerSendUpdateOsirisProfileConfirmation(GSClient* client, bool result);

	bool					ServerSendActivateSpecialAdsToPlayer(GSClient* client, time_t time_end, int specialAdsType);

	bool					ProcessOtherProfileBeforeSending(const GSClient* client, Json::Value& otherProfile);

	bool					ServerSendFinishMinigameReward(GSClient* client, const char* giftType, int quantity); //LNT added for installer minigame reward
	bool					ServerSendEcommResponseChinaRegistration(GSClient* client, int operationType, int status, std::string errorType = "", std::string errorMessage = "");

	bool					ServerSendResponsePersonalInfo(GSClient* client, int status, std::string errorType = "", std::string errorMessage = "");

	Json::Value				CreateFakeBotProfile(const GSClient* client);
	void					AddMPDataToProfile(const GSClient* client, Json::Value* profile);
	void					AddMPTLEDataToProfile(const GSClient* client, Json::Value* profile);
	std::string				GetMPMatcherFilter(const GSClient* client);
	////////////////////////////////////////// CLIENTS LIST MANAGEMENT //////////////////////////////////////////////

	void					HandleNewClients();
	//these methods must be in GameServer class, so they can be overriden in AH and MP GS's
	bool					AddNewClient(const std::string &credential);
	virtual bool			RemoveClient(const std::string &credential);
	bool					RemoveAllClients();
	GSClient*				GetClient(const std::string& credential);
	GSClient*				GetAClient();
	const ClientsListMap&	GetClients();
	const std::string&		GetAccessToken(const std::string& credential);
	void					UpdateAccessToken(const std::string& credential, const std::string& access_token);
	void					UpdateAccessTokenFromJson(const std::string& credential, const Json::Value& json_access_token);
	void					RemoveAccessToken(const std::string& credential);

	void					SafeRemoveClient(const std::string& credential);

	//all operations on players that quit or were disconnected
	void					OnRemoveClient(GSClient* client, const std::string& credential);

	//! Called once per client when we have both seshat and hestia correct data
	void					OnInitialDataRequestsComplete(GSClient* client);

	void					CheckComingSoonBattlepacks(GSClient* client);

	inline time_t			GetLocalTime() const;

	std::string				GetHostname() { return m_hostname; }

	////////////////////////////////////////// CLIENTS LIST MANAGEMENT //////////////////////////////////////////////
	
	//bool					ShouldUserVerifyInstall(const std::string& credential);
	//bool					LoadReceiptValidationFile();

	//void					SetAppReceiptValidation(bool value) { m_appReceiptValidationStatus = value; }
	//bool					IsAppReceiptValidationOn() { return m_appReceiptValidationStatus; }


	////////////////////////////////////////////// SERVER PARAMETERS ////////////////////////////////////////////////
	bool					ServerSendConfigParameters(const std::string& credential);

	bool					ServerSendLPNData(const std::string& credential);

	//////////////////////////////////////////// TIME HACK PREVENTION ///////////////////////////////////////////////

	bool					ServerSendTimeHackDetected(const std::string& credential);

	//////////////////////////////////////////////// COMPATIBILITY //////////////////////////////////////////////////

	bool					ServerSendFoundOpponent(const std::string& credential, const Json::Value& json);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////// FRIENDS ////////////////////////////////////////////////////////////
	//bool					ServerSendFriendsGiftConfigToPlayer(const std::string& credential);
	//bool					ServerSendFriendGiftWasProcessed(const std::string& credential, const std::string& friend_credential);
	bool					ServerSendListConnectionToPlayer(const std::string& credential, const char* listConnectionsJson);
	//bool					UpdateUserStatusLineForToday(std::string &statusLine, bool changeVersion);
	bool					UpdateUserStatusLineLevel(std::string &statusLine, int player_level, int player_prestige_level);
	bool					ServerSendListFriendsGiftsToPlayer(const std::string& credential);
	void					CreateFriendsGiftList(GSClient* client, Json::Value& friendList);
	bool					ServerSendAddConnectionReceivedPlayer(GSClient* client, const char *message);
	void					ServerUpdateFirebaseFriends(GSClient* client);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	void					PostLoginClanOperations(GSClient* client);
	void					ProcessPendingClanCreation(GSClient* client);
	void					ProcessPendingClanRename(GSClient* client);
	bool					ClanClientDataToOperationData(const GSClient* client, const Json::Value& clanData, Json::Value& opData) const;
	static void				ProcessClanEntry(Json::Value* clanEntry, GSClient* forClient, ProcessClanType processClanType = ProcessClanType::Default);
	static void				ProcessTrainerClanEntry(Json::Value* clanEntry, GSClient* forClient);
	static void				ProcessSmallClanEntry(Json::Value* clanEntry);
	static void				ProcessClanRequestEntry(Json::Value* requestEntry);
	static void				ProcessClanSentRequestEntry(Json::Value* requestEntry);
	void					CheckClanEventParticipation(GSClient* client);
	void					CheckRedBossEventParticipation(GSClient* client);
#ifdef RED_BOSS_CLAN_LEADERBOARD
	void					CheckRedBossClanEventParticipation(GSClient* client, const std::string& eventId);
#endif // RED_BOSS_CLAN_LEADERBOARD
#ifdef SKIRMISH_EVENT
	void					CheckSkirmishEventParticipation(GSClient* client);
#endif // SKIRMISH_EVENT

	bool					ServerSendOwnClanToClient(GSClient* client, Json::Value data);
	bool					ServerSendOtherClanToClient(GSClient* client, const Json::Value& data);
	bool					ServerSendClanSearchResultsToClient(GSClient* client, const Json::Value& data);
	bool					ServerSendClansLeaderboard(GSClient* client, const Json::Value& data);
#ifdef RED_BOSS_CLAN_LEADERBOARD
	bool					ServerSendDemonClansLeaderboard(GSClient* client, const Json::Value& data);
#endif // RED_BOSS_CLAN_LEADERBOARD
	bool					ServerSendWallPosts(GSClient* client, const Json::Value& data);
	bool					ServerAppendWallPosts(GSClient* client, const Json::Value& data);
	bool					ServerSendClanWallPostResponse(GSClient* client, const Json::Value& data);
	bool					ServerSendDeleteWallPostResponse(GSClient* client, const Json::Value& data);
	bool					ServerSendRequests(GSClient* client, const Json::Value& data);
	bool					ServerSendSentRequests(GSClient* client, const Json::Value& data);
	bool					ServerSendRequestResult(GSClient* client, const Json::Value& data);
	bool					ServerSendCreateUpdateResult(GSClient* client, ClanCreateUpdateStatus status);
	bool					ServerSendClanBase(GSClient* client, const Json::Value& baseData);
	bool					ServerSendClanDominationLeaderboard(GSClient* client, const Json::Value& lb);
	bool					ServerSendClanModifiers(GSClient* client);
	bool					ServerSendClanCommand(GSClient* client, ClanCommand command, const Json::Value& data);
	bool					ServerSendConquestCommand(GSClient* client, ConquestCommand command, const Json::Value& data);
	//bool					ServerSendSpinWheelWon(GSClient* client, const Json::Value& data);
	bool					ServerSendSpecialAdsWon(GSClient* client, const Json::Value& data, int adsType = 0, const Json::Value& extraData = Json::nullValue); //0 = SPIN_WHEEL, 1 = BP_ADS
	bool					ServerSendChatCommand(GSClient* client, const Json::Value& data);
	bool					ServerSendOwnTrainerClanToClient(GSClient* client, const Json::Value& data);

	bool					ServerSendCurrentConquestStatus(GSClient* client, std::string outpost = "");
	bool					ServerSendCurrentConquestClaimData(GSClient* client);
	bool					ServerClaimConquestData(GSClient* client);

	void					HandleClientClanCommand(GSClient* client, ClanCommand command, Json::Value&& data);

	void					HandleBattlePassCommand(GSClient* client, BattlePassCommand command, Json::Value&& data);

	void					PostClanBossEventScore(const GSClient* client, const Json::Value& jEvents);
	void					PostRedBossEventScore(const GSClient* client, const Json::Value& jEvents);

	void					ServerReadEncryptionKeys(MpCrypto *crypto, GenericDataPacket & message);

	bool					ScriptRegisterForClanWar(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptRegisterForClanWar(const SRequest& requestData);

	bool					ScriptCheckClanWarStart(const GSClient* client, const std::string& forceOpponentClanId = std::string());
	static void				callbackScriptCheckClanWarStart(const SRequest& requestData);

	bool					ScriptCheatAdvanceClanWarTime(const GSClient* client);
	static void				callbackCheatAdvanceClanWarTime(const SRequest& requestData);

	bool					ScriptCheatToggleDominationSettings(const GSClient* client);

	bool					ScriptSetClanJsonField(std::unique_ptr<ClanOperation> operation);
	static void				callbackSetClanJsonField(const SRequest& requestData);

	void					ScriptPostToClanLeaderboard(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptPostToClanLeaderboard(const SRequest& requestData);

	bool					ScriptRegisterForDomination(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptRegisterForDomination(const SRequest& requestData);

	bool					ScriptVoteForDominationZone(std::unique_ptr<ClanOperation> operation);
	static void				callbackScriptVoteForDominationZone(const SRequest& requestData);
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool					LoadCompatibilityVersion();
	int						m_serverCompatibilityVersion;

	const bool				IsRunning() const;

	ServerSettings*					GetServerSettings()			{ return m_serverSettings; }
	const AHWeaponsData*			GetAHWeaponsData()	const;
	const MultiplayerCommonData*	GetMultiplayerData() const	{ return m_multiplayerData; }
	const GachaData*				GetGachaData() const		{ return m_gachaData; }
	GachaData*						GetGachaData()				{ return m_gachaData; }
	const AHMissionsData*			GetAHMissionsData() const;
	const AHEventsData*				GetAHEventsData() const;
	const AHDroneOpsData*			GetAHDroneOpsData() const;
	const FakeNames*				GetFakeNames() const		{ return m_fakeNames; }
	const PvpData*					GetPvpData() const;
	const CollectionCodexData*		GetCollectionCodexData() const { return m_collectionCodexData; }

	const HestiaConfigManager*		GetHestiaConfigManager() const;

	const Json::Value&				GetInitalProfile() const { return m_initial_profile; }

	void							SetClientFedId(GSClient* client, const std::string& fedid);

	bool							IsPvpSelfTest(const GSClient* client) const;

protected:

	void							ResizeBuffers(size_t size);

	ClientsListMap m_gsClientsList;
	ClientsListMap m_gsClientsListByFedCredential;
	std::map<std::string, std::string>		m_gsAccessToken;
	time_t m_idleQuitTimestamp = 0;

	std::vector<unsigned char> m_recvBuffer;
	std::vector<unsigned char> m_tempBuffer;

	int						GetRecvBufferSize() const { return (int)m_recvBuffer.size(); }
	int						GetTempBufferSize() const { return (int)m_tempBuffer.size(); }

	std::string				m_newClientCredential;

	std::string				m_hostname;
	std::string				m_revision;

	Json::Value				m_initial_profile;
	Json::Value				m_customJson;
	Json::Value				m_loadingHintsJson;
	Json::Value				m_chancesJson;
	Json::Value				m_serverConfigParamsJson;
	Json::Value				m_splashScreensJson;
	Json::Value				m_motdData;
	Json::Value				m_uuids;
	Json::Value				m_requiredDlcVersion;

	ServerSettings*			m_serverSettings;
	MultiplayerCommonData*	m_multiplayerData;
	GachaData*				m_gachaData;
	FakeNames*				m_fakeNames;
	CollectionCodexData*	m_collectionCodexData;

	std::unique_ptr<const AHMissionsData>	m_missionsData;
	std::unique_ptr<const AHWeaponsData>	m_weaponsData;
	std::unique_ptr<const AHEventsData>		m_eventsData;
	std::unique_ptr<const AHDroneOpsData>	m_droneOpsData;
	std::unique_ptr<const PvpData>			m_pvpData;

	std::unique_ptr<HestiaConfigManager>	m_hestiaConfigManager;

	bool					m_appReceiptValidationStatus;
	std::vector<std::string> m_tbcUsersList;	

	std::vector<std::string> m_safeRemoveClients;

	void UpdateClientsEventShopLimits();
	void UpdateAutojoinableEvents();

	void UpdateClientsBattlePass();
	void InitBattlePass(GSClient* client);
	void ResetBattlePass(GSClient* client);

	void BroadcastRequiredDlcVersion();
	bool ServerSendRequiredDlcVersion(GSClient* client);

	bool ServerSendForgeSquadmatesPacks(GSClient* client);
	void CreateForgeSquadmatesPacks(GSClient* client);
	bool ServerSendForgeRewardActivated(GSClient* client, byte state);
	bool ServerSendForgeFuelsWeapons(GSClient* client);

public:
	void ResetClanBattlePass(GSClient* client);

	struct Forge_Squadmates_Packs
	{
		StringHash pack_id;
		std::vector < StringHash> squads;
	};
	std::vector <Forge_Squadmates_Packs> m_ForgeSquadsToSend;
	void ActivateForgeRewardList(GSClient* client, byte activate);
	
	std::string DefaultSurvivalWeapon(std::string tag) const;
	std::string DefaultSurvivalGearset(std::string tag) const;

	// --- Survival Rotating Leaderboard
	double GetSurvivalProgressByLBTypeFromStatistic(GSClient* client, E_SurvivalLeaderboardType lbType, const Json::Value& jStatistics, const std::string& eventID) const;
	double ComputeSurvivalScore(E_SurvivalLeaderboardType lbType, double newScore, double currentScore) const;
public:
	void AddBattlePassProgressFromStatistics(GSClient* client, const Json::Value& jStatistics);
	void CheckBattlePassLeague(GSClient* client, int league = 0, bool trackChanges = false);
	void BattlePassRefresh(GSClient* client, bool trackChanges = false);

protected:
	void									AddReferral(GSClient* client, const char* code);
	

	Json::Value								m_LPNSettings;

public:
	void									UpdateReferralUser(GSClient* client);
	void									UpdateRecruitsGifts(GSClient* client);
	std::string								m_playerStatusLine;
	void									UpdateBPFreeGifts(GSClient* client, int reward_type);
	void									GiveArenaTokenFreeGifts(GSClient* client);
	void									GiveAdsReward(GSClient* client,StringHash tranzId, std::string rewardName, int rewardAmount);
	void									ReduceDelayedTransactionsTime(GSClient* client, const std::string& item_currency);

//////////////////////////////////////////////////// TRACKING ////////////////////////////////////////////////////////

private:
	struct IPR_Param
	{
		StringHash id;
		bool suppress_no_change_error = false;

		bool req = false;
		bool pending = false;
	};
	using IPR_QueueMap = mcfwutil::fsmap<std::string, IPR_Param>;
	IPR_QueueMap m_IPR_Queue;

public:

	time_t m_lastSentEtsEvents = 0;
	std::vector<tracking::TrackingRequest> m_etsEvents;
	
	void									UpdatePendingEtsRequests();

	void									UpdateDeputies();
	void									UpdateCommanders();

	void									UpdateWarlord();
	void									UpdateDailyShop();
	void									UpdateClanBattlepass();
	void									UpdateArenaModsExpiry();
	void									UpdateIAPPendingRequests();
	void									QueueIAPPendingRequests(GSClient* client, StringHash id, bool suppress_no_change_error);
	void									SetIAPPendingRequest(GSClient* client, bool pending);
	IPR_Param&								GetIAPPendingRequest(GSClient* client);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
	struct TReloadData
	{
		using LOADER = bool (GameServerFedManager::*)();

		time_t								timestamp;
		const char*							file;
		LOADER								LoadFn;
	};

	static std::vector<TReloadData>			m_reloadData;

	void									InitFilesReloadCheck();
	void									CheckFilesToReload();

public:
	static GameServerFedManager*			m_instance;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	static inline time_t											GetCrtUTCTimeSeconds();
	static std::chrono::time_point<std::chrono::system_clock>		GetCrtUTCTime();

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	void MissionCompletedRewards(GSClient* client, MissionUid muid, hash_t weapon, bool findPartsMissionReward, const std::string& eventId, const TStatistics<double>* statistics, int64_t* XP, int64_t* SC) const;
	void ActivateRaidTicket(GSClient* client, hash_t partId, bool contract);
	void AddLeagueRewards(GSClient* client, StringHash weapon, Json::Value& jResponse) const;
	void ApplyModifiers(const GSClient* client, StringHash weapon, const char* param, int64_t* val, ModifierType type = ModifierType::_Default) const;
	void ApplyPvpModifiersOnPvpStart(Json::Value* op_json, GSClient* client, E_OpponentType oppoType) const;
	bool DropMissionEndStuff(GSClient* client, const Json::Value* dropPartsOverride, StringHash findPartsMissionPart, const MissionDropChancesData& dropData) const;
	void GiveEndMissionPvpPacksLogic(GSClient* client, bool doubleReward = false);
	void GiveEndMissionArenaTokens(GSClient* client, bool doubleReward = false);
#ifdef SKIRMISH_EVENT
	void GiveEndMissionSkirmishTokens(GSClient* client, uint8_t bet = 1);
#endif // SKIRMISH_EVENT

protected:
	void ApplyPercentMoreModifier(const GSClient* client, StringHash weapon, const char* param, int64_t* val, ModifierType type = ModifierType::_Default) const;

protected:
	MissionDropChancesData MissionDropChances(GSClient* client, MissionUid muid, const std::string& eventId = "") const;
	void DropClanKeys(GSClient* client, const Json::Value* drops, double dropsFactor);

	struct FindPartsData
	{
		const char* weaponClass;
		StringHash battlepack;
	};
	FindPartsData FindPartsDataForPart(GSClient* client, hash_t partId) const;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// --- event mission params
// --- event missions
private:
	struct EventMissionValidParams
	{
		std::vector<const char*> weaponClasses;
		bool miniopsSetup = false;

		bool valid() const { return !weaponClasses.empty(); }
		bool valid(const GSClient* client, const Json::Value& params) const;
	};

	bool					UpdateMissionParamsForEvent(GSClient* client, const SemEvent& ev);
	bool					EventRequiresMissionParams(const GSClient* client, const SemEvent& ev) const;
	bool					EventConditionsMet(GSClient* client, const SemEvent& ev) const;
	EventMissionValidParams	ValidParamsForEventMission(GSClient* client, const SemEvent& ev) const;

public:
	void					UpdateEventsMissionParams(GSClient* client);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// --- elo ladder expiry information

protected:
	struct LeaderboardExpiryEntry
	{
		LeaderboardExpiryEntry(time_t expiry, const char* id = "", 
			EloSettings eloSettings = EloSettings::Count, 
			const char* pvpSettings = "", int pvpWeekHours = 0,
			bool comingSoon = false, const char* clanLb = "",
			bool decay = false, const char* skmSettings = "")
			: expiry(expiry)
			, id(id)
			, settings(eloSettings)
			, pvpSettings(pvpSettings)
			, pvpWeekHours(pvpWeekHours)
			, comingSoon(comingSoon)
			, clanLb(clanLb)
			, decay(decay)
			, skmSettings(skmSettings) {}
		time_t expiry;
		std::string id;
		EloSettings settings;
		std::string pvpSettings;
		std::string skmSettings;
		int pvpWeekHours;
		bool comingSoon;
		std::string clanLb;
		bool decay;

		bool operator< (const LeaderboardExpiryEntry& other) const
		{
			return expiry < other.expiry;
		}
	};
	std::vector<LeaderboardExpiryEntry> m_eloLadderExpiryData;
	const LeaderboardExpiryEntry* GetCurrentLadderExpiryEntry() const;
	const LeaderboardExpiryEntry* GetLadderExpiryEntry(time_t timestamp) const;
	EloSettings GetEloSettingsForLadder(const std::string& ladderId) const;
	time_t m_nextLadderCheck;
	time_t m_nextPVPWeekCheck;

	bool ServerSendLadderProps(const GSClient* client);
	bool ServerSendLadderExpiry(const GSClient* client);
	bool ServerSendPVPData(const GSClient* client);
	void UpdateClientsLadderExpiry();
	void UpdateClientsPVPWeekExpiry();

public:
	time_t GetCurrentLadderExpiry() const;
	time_t GetCurrentLadderStart() const;
	const char* GetLastLadderId() const;
	const char* GetCurrentLadderId() const;
	EloSettings GetCurrentLadderSettings() const;
	const char* GetLadderPvpSettings(const std::string& ladderId) const;
	const char* GetCurrentLadderPvpSettings() const;
	const char* GetCurrentLadderSkmSettings() const;
	std::string GetClanLb(const std::string& ladderId) const;
	std::string GetCurrentClanLb() const;
	std::string GetClientClanLb(const std::string& ladderId) const;
	std::string GetCurrentClientClanLb() const;
	bool HasCurrentLadderDecay() const;
	bool HasLastLadderDecay() const;
	const char* GetLastLadderWithDecay() const;
	int GetLadderPvpWeekHours(const std::string& ladderId) const;
	int GetCurrentLadderPvpWeekHours() const;
	bool GetCurrentLadderComingSoon() const;
	std::pair<int, time_t> GetLadderPvpWeek(const char* ladderId = nullptr) const;
	const char* GetLadderId(time_t timestamp) const;
	time_t GetLadderStart(const std::string& ladderId) const;
	time_t GetLadderExpiry(const std::string& ladderId) const;

	bool IsEloRanked(const Json::Value& pvp_stats) const;
	bool IsClientEloRanked(const GSClient* client) const;

private:

	std::map<StringHash, CachedClan>	m_clansCache;
	void								ParseClanDataForCache(const Json::Value& clanData);
	const CachedClan*					GetCachedClan(StringHash clanId) const;

// --- Skirmish event information
#ifdef SKIRMISH_EVENT
protected:
	struct SkirmishSeasonData
	{
		SkirmishSeasonData(const char* id = "", bool active = false, time_t ss_end = 0, time_t token_expiry = 0) : seasonId(id), seasonActive(active), seasonEnd(ss_end), tokenExpiry(token_expiry) {}
		std::string seasonId;
		bool		seasonActive;
		time_t		seasonEnd;
		time_t		tokenExpiry;
	};
	SkirmishSeasonData	m_skirmishSeasonData;
	Json::Value			m_skirmishScheduleData;
	bool ServerSendSkirmishSchedule(const GSClient* client, Json::Value& data);
	void SetSkirmishSeasonData(const Json::Value& data);
public:
	std::string GetSkirmishSeasonId() const { return m_skirmishSeasonData.seasonId; }
	bool		IsSkirmishSeasonActive() const { return m_skirmishSeasonData.seasonActive; }
	time_t		GetSkirmishSeasonEndTime() const { return m_skirmishSeasonData.seasonEnd; }
	time_t		GetSkirmishTokenExpiryTime() const { return m_skirmishSeasonData.tokenExpiry; }
#endif // SKIRMISH_EVENT

// --- clan battlepass
public:
	void SendCBDataToClanServer(const std::string& credential);

// --- squad ops
public:
	enum class SquadOpParamsFor
	{
		Client,
		Server,
	};
	enum class NewSquadOp
	{
		WithDelay,
		WithoutDelay,
	};
	int HighestSquadOpChapter(GSClient* client) const;
	void EnsureSquadOpMissions(GSClient* client);
	void ValidateSquadOpsMissions(GSClient* client, int chapter);
	void RefreshSquadOpsMissions(GSClient* client, int chapter);
	void EnsureSquadOpsMissionsForChapter(GSClient* client, int chapter, NewSquadOp delay);
	void AddSquadOpsMissionForChapter(GSClient* client, int chapter, NewSquadOp delay);
	Json::Value ComputeSquadOpParams(GSClient* client, const Json::Value& jConfig, SquadOpParamsFor paramsFor);
	bool ServerSendSquadOpsCommand(GSClient* client, SquadOpsCommand command, const Json::Value& data);
	void StartSquadOps(GSClient* client, const Json::Value& jConfig);
	void ComputeAndSetSquadOpsOutcome(GSClient* client);
	void ClaimSquadOps(GSClient* client, const Json::Value& jConfig);
	void ClaimAllSquadOps(GSClient* client);
	Json::Value FinishSquadOp(GSClient* client, const std::string& missionId, time_t timeOffset = 0);
	std::string SetupTutorialSquadOp(GSClient* client);
protected:
	struct SquadOpsRandomParams;
	SquadOpsRandomParams RandomParamsForSquadOpsChapter(GSClient* client, int chapter) const;

//pvp slots menu
	void MergePvpSlots(GSClient* client, const char* offerName);
	void OpenPvpSlot(GSClient* client, const char* offerName);
	void RushPvpSlots(GSClient* client, const char* offerName);

// --- motd
protected:
	Json::Value m_dayMotd = Json::nullValue;
	time_t m_nextMotdCheck = 0;

	void UpdateMotd();
	bool CheckSendMotd(GSClient* client, const Json::Value& motdData);

// --- top / hof leaderboard data
public:
	//struct TopLeaderboardData
	//{
	//	const int HOF_ENTRIES = 200;	// entries
	//	const int TOP_ENTRIES = 200;	// entries
	//	const int REQUEST_LIMIT = 500;	// entries
	//	const int FAIL_COUNT = 2;		// no of fail after which a timeout will be added
	//	const int FAIL_TIMEOUT = 10;	// seconds
	//	const int REQUEST_TOO_LONG = 60;// seconds

	//	TopLeaderboardData(GameServerFedManager& mgr)
	//		: mgr_(mgr),
	//		incomingData(),
	//		currentData(),
	//		reqEntries(TOP_ENTRIES + HOF_ENTRIES),
	//		lastCheck(0),
	//		opCount(0),
	//		failCount(0),
	//		failTimeout(0)
	//	{ }

	//	struct LbData
	//	{
	//		time_t cookie;
	//		Json::Value lb;	// actual lb data (processed)
	//		int hofEntries;	// entries in hof
	//		int topEntries; // entries in top
	//		int minElo;	// min elo in lb
	//		std::set<StringHash> credInTop;	// fed credentials in lb
	//		std::map<StringHash, Json::Value> hiddenLb; // hidden hof entries
	//		std::map<int, Json::Value> lbByRank; // lb grouped by first rank
	//		bool mandatory;	// this lb is a mandatory update for every client

	//		void reset()
	//		{
	//			lb = Json::nullValue;
	//			topEntries = 0;
	//			hofEntries = 0;
	//			minElo = 0;
	//			cookie = 0;
	//			credInTop.clear();
	//			hiddenLb.clear();
	//			lbByRank.clear();
	//			mandatory = false;
	//		}

	//		LbData()
	//		{
	//			reset();
	//		}
	//	} currentData, incomingData;

	//	GameServerFedManager& mgr_;

	//	int reqEntries;	// entries to request
	//	time_t lastCheck;	// ts for last data sent to clients
	//	int opCount; // no of ops launched for a lb-get

	//	int failCount;	// no of consecutive requests failed
	//	time_t failTimeout;	// wait to this ts to retry after a fail

	//	std::map<std::string, time_t> clientWaitingList;	// clients waiting for a lb refresh (with their request ts)

	//	void process_data(const Json::Value& data);
	//	bool send_data_to_client(GSClient* client);
	//	void should_get_top(const GSClient* client, std::unique_ptr<LadderOperation> operation);
	//	void get_top(const GSClient* client);
	//	void update();
	//	void get_fail(time_t cookie);
	//};

	//TopLeaderboardData		m_topLeaderboard;

// --- new Elo ladder data
	struct EloLadder
	{
		EloLadder(GameServerFedManager& mgr, StringHash ladderId, EloSettings eloSettings);
		virtual ~EloLadder() {}

		virtual void refresh_for_client(GSClient* client) = 0;
		virtual void on_get_error(std::unique_ptr<LadderOperation> operation) = 0;
		virtual void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply) = 0;
		virtual void on_post(GSClient* client, std::unique_ptr<LadderOperation> operation) = 0;
		virtual void on_delete_from_top(std::unique_ptr<LadderOperation> operation) = 0;
		virtual void additional_post_op(std::unique_ptr<LadderOperation> operation, std::map<std::string, std::string>&& customParams) = 0;

		StringHash current_ladder_id() const { return current_ladder_id_; }
		int min_hof_elo() const { return min_hof_elo_; }
		EloSettings current_elo_settings() const { return current_elo_settings_; }

	protected:
		GameServerFedManager& mgr_;
		StringHash current_ladder_id_;
		EloSettings current_elo_settings_ = EloSettings::Count;
		int min_hof_elo_ = 3400;
	};

	struct UnlimitedHofEloLadder : EloLadder
	{
		static const int HOF_ENTRIES = 200;	// entries
		static const int TOP_ENTRIES = 200;	// entries

		UnlimitedHofEloLadder(GameServerFedManager& mgr, StringHash ladderId, EloSettings eloSettings)
			: EloLadder(mgr, ladderId, eloSettings) {}

		void refresh_for_client(GSClient* client) override;
		void on_get_error(std::unique_ptr<LadderOperation> operation) override;
		void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply) override;
		void on_post(GSClient* client, std::unique_ptr<LadderOperation> operation) override;
		void on_delete_from_top(std::unique_ptr<LadderOperation> operation) override;
		void additional_post_op(std::unique_ptr<LadderOperation> operation, std::map<std::string, std::string>&& customParams) override;

	private:
		void send_data_to_client(GSClient* client) const;
		void send_data_to_all_clients() const;

		int lowest_top_score_ = 0;
		int lowest_hof_score_ = 0;
		Json::Value top_players_;
		Json::Value hof_players_;
		time_t ts_top_ = 0;
		time_t ts_hof_ = 0;
	};

	struct LimitedHofEloLadder : EloLadder
	{
		static const int TOP_ENTRIES = 200;	// entries

		LimitedHofEloLadder(GameServerFedManager& mgr, StringHash ladderId, EloSettings eloSettings, int hofEntries)
			: EloLadder(mgr, ladderId, eloSettings)
			, max_hof_entries_(hofEntries)
		{ }

		void refresh_for_client(GSClient* client) override;
		void on_get_error(std::unique_ptr<LadderOperation> operation) override;
		void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply) override;
		void on_post(GSClient* client, std::unique_ptr<LadderOperation> operation) override;
		void on_delete_from_top(std::unique_ptr<LadderOperation> operation) override;
		void additional_post_op(std::unique_ptr<LadderOperation> operation, std::map<std::string, std::string>&& customParams) override;

	private:
		void send_data_to_client(GSClient* client) const;
		void send_data_to_all_clients() const;

		int lowest_top_score_ = 0;
		int highest_top_score_ = 0;
		Json::Value players_;
		time_t ts_ = 0;
		int max_hof_entries_ = 0;
	};

	void SetEloLadderProps(StringHash ladderId, EloSettings eloSettings);
	std::unique_ptr<EloLadder> m_eloLadder;

	struct MPTopLeaderboard
	{
		static const int TOP_ENTRIES = 200;	// entries

		MPTopLeaderboard(GameServerFedManager& mgr)
			: mgr_(mgr)
		{ }

		void refresh_for_client(GSClient* client);
		void on_get_error(std::unique_ptr<LadderOperation> operation);
		void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply);
		void on_post(GSClient* client, std::unique_ptr<LadderOperation> operation) ;

	private:
		void send_data_to_client(GSClient* client) const;
		void send_data_to_all_clients() const;

		GameServerFedManager& mgr_;
		int lowest_top_score_ = 0;
		int highest_top_score_ = 0;
		Json::Value players_;
		time_t ts_ = 0;
	};

	std::unique_ptr<MPTopLeaderboard> m_mpTop;

	struct OutpostsTopLeaderboard
	{
		static const int TOP_ENTRIES = 200;	// entries

		OutpostsTopLeaderboard(GameServerFedManager& mgr, const std::string& lbId)
			: mgr_(mgr), lbId_(lbId)
		{ }

		void refresh_for_client(GSClient* client, bool sendDataToClient = false);
		void on_get_error(std::unique_ptr<LadderOperation> operation);
		void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply);
		void on_post(GSClient* client, std::unique_ptr<LadderOperation> operation);

	private:
		void send_data_to_client(GSClient* client, bool sendDataToClient = false) const;
		void send_data_to_all_clients() const;

		Json::Value get_data(const GSClient* client) const;

		GameServerFedManager& mgr_;
		std::string lbId_;
		int lowest_top_score_ = 0;
		int highest_top_score_ = 0;
		Json::Value players_;
		std::map<StringHash, Json::Value> jPlayers_ = {};
		time_t ts_ = 0;
		bool getting_ = false;
		std::set<std::string> clients_ = {};
	};

	std::map<StringHash, OutpostsTopLeaderboard> m_outpostsTops;
	OutpostsTopLeaderboard& GetOutpostsTop(const std::string& lbId);
	void RefreshOutpostsTop(GSClient* client, const std::string& lbId);

	struct MPArenaLeaderboard
	{
		static const int TOP_ENTRIES = 200;	// entries

		MPArenaLeaderboard(GameServerFedManager& mgr)
			: mgr_(mgr)
		{ }

		void refresh_for_client(GSClient* client);
		void on_get_error(std::unique_ptr<LadderOperation> operation);
		void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply);
		void on_post(GSClient* client, std::unique_ptr<LadderOperation> operation);

	private:
		void send_data_to_client(GSClient* client) const;
		void send_data_to_all_clients() const;

		GameServerFedManager& mgr_;
		int lowest_top_score_ = 0;
		int highest_top_score_ = 0;
		Json::Value players_;
		time_t ts_ = 0;
	};

	std::unique_ptr<MPArenaLeaderboard> m_mpArena;

	struct MPTop3Leaderboard
	{
		static const int TOP_ENTRIES = 3;	// entries

		MPTop3Leaderboard(GameServerFedManager& mgr, const std::string& lbId)
			: mgr_(mgr), lbId_(lbId)
		{ }

		void refresh_for_client(const GSClient* client, bool sendDataToClient = true);
		void on_get_error(std::unique_ptr<LadderOperation> operation);
		void on_data(std::unique_ptr<LadderOperation> operation, Json::Value&& jReply);

		int get_rank(const GSClient* client) const;

	private:
		void send_data_to_client(const GSClient* client, bool sendDataToClient = true) const;
		void send_data_to_all_clients();

		GameServerFedManager& mgr_;
		std::string lbId_;
		int lowest_top_score_ = 0;
		int highest_top_score_ = 0;
		std::map<StringHash, int> players_ = {};
		Json::Value jPlayers_;
		bool getting_ = false;
		std::set<std::string> clients_ = {};
	};

	std::map<StringHash, MPTop3Leaderboard> m_mpTop3;
	MPTop3Leaderboard& GetMPTop3(const std::string& lbId);
	void RefreshMPTop3(const GSClient* client, const std::string& lbId);

// --- bounty
public:
	void ApplyBountyRewardRules(GSClient* client, const std::string& eventId, int chapter, StringHash id, int64_t* qty) const;
	int HighesteCompletedBountyChapter(GSClient* client) const;
protected:
	void SetupBountyChapter(GSClient* client, const ServerEvent* evt, int chapter, BountyChapterOp ops);
	void ResetBountyEvent(GSClient* client, const ServerEvent* evt);

// --- miniops
public:
	void UpdateMiniopsPveRewards(GSClient* client, const ServerEvent* evt = nullptr);

// --- mods
public:
	void ResetOwnedTempMods(GSClient* client);
	void ResetOwnedExpMods(GSClient* client);

protected:
	void ChatCommand(GSClient* client, const Json::Value& data);

// --- get more items sources
private:
	void HandleShopCommand(GSClient* client, ShopCommand command, const Json::Value& commandData);
	bool ServerSendShopCommand(GSClient* client, ShopCommand command, const Json::Value& data);
	void ComputeGetMoreItemSources(GSClient* client, const Json::Value& commandData);
	bool CheckInCategory(GSClient* client, StringHash categoryId, const char* id, int qty);

	void HandleMPCommandFromClient(GSClient* client, mpserver::MPAHMessageId command, const Json::Value& commandData);
	bool ServerSendMPCommand(GSClient* client, NetworkMessageW& message);

	void HandleAutomatchCommand(GSClient* client, AutomatchCommand command, const Json::Value& commandData);
	bool ServerSendAutomatchCommand(GSClient* client, AutomatchCommand command, const Json::Value& data = Json::nullValue);

protected:
	bool ServerSendMpRankingsUpdate(GSClient* client);

private:
	void TestFakeClient();

// --- ads config
public:
	void GiveAdsRewardTest(GSClient* client, Json::Value& rewardInfo);
	bool SendShopDailyAds(GSClient* client, int shopDailyCount, unsigned int cooldownEnd);

	void GiveShopDailyAdsReward(GSClient* client);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	static GameServerFedManager*			Instance()		{ return m_instance; }

protected:
	std::vector<unsigned char> m_strBuff;

private:
	std::unique_ptr<comms::manager> m_commsManager;

private:
	void UpdateComms();
	void UpdateMPServer();
	void HandleMPMessage(GSClient* client, comms::read_message message);
	void HandleMPDisconnect(GSClient* client, comms::Connection_error reason, int error);
	void HandleMPDisconnect(comms::connection q, comms::Connection_error reason, int error);
	void SendDisconnectRequestToClient(GSClient* client, mpserver::RequestDisconnectReason reason);
	void SendDisconnectRequestToMP(GSClient* client, mpserver::RequestDisconnectReason reason);

private:
	char* m_cheatersData = nullptr;
	char* m_cheatersData2 = nullptr;

	charPSetT *m_cheaters = nullptr;
	charPSetT* m_cheaters2 = nullptr;

//Huong.DaoNgoc add for checking beta environment
public:
	bool IsBetaEnvironment();
};

inline time_t GameServerFedManager::GetLocalTime() const
{
	return m_serverTime;
}

inline time_t GameServerFedManager::GetCrtUTCTimeSeconds()
{
	return GameServerFedManager::Instance()->m_serverTime;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}//namespace antihackgs

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#endif // __GAMESERVER_FED_MANAGER_H__

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
