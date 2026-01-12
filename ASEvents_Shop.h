
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef AS_EVENTS_SHOP_H_INCLUDED
#define AS_EVENTS_SHOP_H_INCLUDED

#include "OnlineManagers/IapManager.h"

namespace gameswf
{
	struct ASObject;
	struct ASArray;
	struct Player;
}

struct ShopOffer;

enum class BattlepackType
{
	Ordinary,
	Arena,

	_count,
};

enum class TicketsShopType
{
	TicketsShop,
	SurvivalShop,
	SkirmishShop,
};

enum class BundleListFilter
{
	Regular,
	Arena,
	Daily,
	Events,
	Rental
};

enum BundleListFlags
{
	None			= 0,
	ShopV2			= 1 << 0,
	CheckExistOnly = 1 << 1,
	CheckPromoOnly	= 1 << 2,
};

enum ShopV2Flags
{
	ShopV2None = 0,
	ShopV2Exist = 1 << 1,
	ShopV2Promo = 1 << 2,
};

enum class SwfShopMenu : int
{
	// --- these have direct mappings in the old shop and the new shop
	Battlepacks,
	Bundles,
	BlackMarket,
	Credits,
	CreditsIngame,
	Weapons,
	// --- these only exist in the new shop; they will redirect to the bundles section of the old shop
	Daily,
	Events,
	Rental,
};

void Shop_SendMarketData(std::string reqCurrency = "", int minimumRequired = 0 , bool IsTrue = false);
void Shop_SendHcMarketData(StringHash category);
void Shop_SendShopSpecialOffers();
void Shop_SendBattlepacks(const std::string &id, bool updateLast = false);	// --- if update last is true, ignore the call if packId changed since the last call.  Used from UpdateShopInventoryImpl
void Shop_UpdateBattlepacksSelection(const char* packId);
int  Shop_ComputeAndSendBattlepacksInitData(bool send, BattlepackType type);
void Shop_SendBattlepackRewards(const Json::Value& invUpdateJson, StringHash tranzId);
void Shop_SendBattlepackArenaTutorialRewards();
void Shop_SendRaidRewards(const Json::Value& invUpdateJson);
void Shop_SendBundlesData(bool isArenaBundle, bool shopV2 = false);
void Shop_SendCreditsShopData();
void Shop_ShowBundleConfirmationPopup(StringHash offerId);
bool Shop_RefreshEnergyPopup();
void Shop_ShowSkipEventMissionRewardsPopup(const Json::Value& invUpdateJson, const std::string& eventType);
void Shop_SendShopBpData(StringHash category);
void Shop_sendBlackMarketData();
void Shop_sendTicketShopData(TicketsShopType type = TicketsShopType::TicketsShop);
#ifdef SKIRMISH_EVENT
void Shop_sendSkirmishShopData();
#endif // SKIRMISH_EVENT
void Shop_sendArmsDealerData();
void Shop_showBuyConsumablePopup(const ShopOffer* offer);
void Shop_ShowRendEndedPopup(std::vector<unsigned int> itemsRent);
void Shop_playAnimationBlackMarketData();
// Shop V2
void ShopV2_SendDailyData(BundleListFlags flags = BundleListFlags::ShopV2);
void ShopV2_SendRentalData(BundleListFlags flags = BundleListFlags::ShopV2);
void ShopV2_EquipRentedItem(int itemId);
int ShopV2_CheckBMOfferList(gameswf::Player* swfPlayer, const char* bmPrefix);
int ShopV2_SendBlackMarketData(ShopV2Flags flags = ShopV2Flags::ShopV2None);
void ShopV2_playAnimationBlackMarketData();
int ShopV2_SendMarketData(gameswf::ASArray* currencyPacksSwf, ShopV2Flags flags, StringHash& descriptionId, std::string reqCurrency, int minimumRequired, bool skipShowSubcription);
void ShopV2_SendHcMarketData(gameswf::ASArray* currencyPacksSwf, StringHash& descriptionId, StringHash category);
int ShopV2_AskForShopPacksData(ShopV2Flags flags, const std::string& id = "", const int minimumRequired = 0, bool skipShowSubcription = false);
int ShopV2_SendPacksData(gameswf::ASArray* currencyPacksSwf, ShopV2Flags flags, StringHash& descriptionId, const char* category = nullptr);
int ShopV2_ComputeAndSendBattlepacksInitData(bool send, ShopV2Flags flags, BattlepackType type);
void ShopV2_AddBattlepackData(const std::string& packID, gameswf::ASObject* asPack);
void ShopV2_SendEventsData(BundleListFlags flags = BundleListFlags::ShopV2);
void BattlepacksBuyPack(int id, int quantity, bool isTutorial);

gameswf::ASObject* ConstructOffIAPPromo(const IapManager::StoreItem* item, gameswf::Player* fxPlayer);
gameswf::ASObject* ConstructTimeLeft(int seconds, gameswf::Player* fxPlayer);
gameswf::ASObject* ConstructIAPTimeLeft(gameswf::Player* fxPlayer);
gameswf::ASObject* ConstructOfflineItemsTimeLeft(gameswf::Player* fxPlayer, StringHash offerId);
gameswf::ASObject* ConstructGAMETimeLeft(gameswf::Player* fxPlayer);
gameswf::ASObject* ConstructIAPItemOutsideShop(int itemIdx, const Json::Value& extra_category, bool hasPromo, double smallest_price, int smallest_amount, gameswf::Player* fxPlayer, bool skipShowSubcription, bool shopV2 = false);
int GetOfflineItemsTimeLeft(StringHash offerId);

class BundleListArray
{
	gameswf::ASArray* arr;
	gameswf::Player* fxPlayer;

	std::vector<StringHash> added_credits;
	std::vector<StringHash> credits_to_skip;

	int additionalInfoFlags;

public:
	BundleListArray(gameswf::Player* fxPlayer, int additionalInfoFlags = 0);
	void AddItem(StringHash hash, int qty);
	gameswf::ASArray* GetArray() { return arr; }
};

void Shop_ShowGetMorePopup(const std::string& itemType);
void Shop_RefreshGetMorePopup(const Json::Value& reply);

//AREna
void Shop_SendArenaPacksData(const std::string &packID);

int ComputeCustomTimer(int itemIdx);

gameswf::ASObject* Shop_CreateIAPGift(int giftID, const std::string& giftName, gameswf::Player* fxPlayer);
auto Shop_GetIAPGift(const Json::Value& array)->std::pair<int, std::string>;

const char* GetShopMenuName(SwfShopMenu swfMenu);
bool BundleHasExtracategory(const Json::Value& extra_category, const char* tag);

#endif	// !AS_EVENTS_SHOP_H_INCLUDED
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
