BundlesData Shop_ConstructBundlesData(gameswf::Player* fxPlayer, BundleListFilter filter, BundleListFlags flags)
{
	BundlesData ret;
	auto consumables = swfnew gameswf::ASArray(fxPlayer);

	// Helper function
	auto IsKeyInArray = [](StringHash key, const std::vector<StringHash>& arr)
	{
		return std::find(arr.begin(), arr.end(), key) != arr.end();
	};

	// --- IAP bundles first
	{
		for (unsigned i = 0; i < IAP()->GetItemsCount(); ++i)
		{
			const auto crmi = IAP()->GetCRMItem(i);
#if !defined(OS_ANDROID) || USE_IN_APP_BILLING_CRM //truong.px4 add for GAND compile
			if (crmi->IsHidden())
				continue;
#endif

			const std::string& type = crmi->GetType();
			if (!crmi->HasBundleItems())
				continue;

			if (crmi->GetBillingMethodCount() < 1)
				continue;

			if (IAP()->GetBundleOldSubscription(i))	// --- subscriptions are in hc section
				continue;

			Json::Value jex;
			auto ex = crmi->GetExtendedField("category");
			if (ex.IsValid() && ex.IsJSON())
			{
				auto sjex = ex.ToString();
				Json::Reader rd;
				rd.parse(sjex, jex);
			}

			if (filter == BundleListFilter::Arena && !BundleHasExtracategory(jex, SHOP_ITEM_XTRA_ARENA_BUNDLE))
				continue;

			if ((filter == BundleListFilter::Daily && !BundleHasExtracategory(jex, SHOP_ITEM_XTRA_DAILY_BUNDLE)) ||
				(filter != BundleListFilter::Daily && BundleHasExtracategory(jex, SHOP_ITEM_XTRA_DAILY_BUNDLE)))
			{
				continue;
			}

			if ((filter == BundleListFilter::Events && GetBundleExtraCategoryValue(jex, SHOP_ITEM_XTRA_EVENT_TYPE_BUNDLE).empty()) ||
				(filter != BundleListFilter::Events && !GetBundleExtraCategoryValue(jex, SHOP_ITEM_XTRA_EVENT_TYPE_BUNDLE).empty()))
			{
				continue;
			}

			if ((filter == BundleListFilter::Rental && !BundleHasExtracategory(jex, SHOP_ITEM_XTRA_RENT_BUNDLE)) ||
				(filter != BundleListFilter::Rental && BundleHasExtracategory(jex, SHOP_ITEM_XTRA_RENT_BUNDLE)))
			{
				continue;
			}

			// --- TODO: forced promo layout
			if (crmi->HasPricePromotion())
				ret.hasIAPPromo = true;

			auto itemSwf = CreateBundleIAPItem(i, jex, fxPlayer, flags);

			if (itemSwf)
			{
				ret.hasIAPBundle = true;

				// --- early out in case we just want to see if there are any items here
				if (flags & BundleListFlags::CheckExistOnly)
					return ret;

				if (GetShopManager()->HasPromoTag(EPROMOTAG_MSG_IAP, IAP()->GetItemHashId(crmi)))
				{
					ret.hasIAPPromo = true;
					itemSwf->setMemberByName(KSTR("description"), GetShopManager()->GetIAPPromoDescription(IAP()->GetItemHashId(crmi)));
				}

				// rental shop IAP bundle
				if (filter == BundleListFilter::Rental)
				{
					itemSwf->setMemberByName("rentalShop", true);
					for (int i = 1;; i++) 
					{
						auto xtra = "rent_bundle_" + std::to_string(i);
						auto xtraOffer = GetShopManager()->FindShopOffer(StringHash(xtra.c_str()));

						if (!xtraOffer)
							break;
						
						if (BundleHasExtracategory(jex, xtra.c_str()) && xtraOffer && IsKeyInArray(SHOP_ITEM_XTRA_RENT_BUNDLE, xtraOffer->xtra))
						{
							auto rentOffer = CreateBundleObject(fxPlayer, xtraOffer, flags);
							itemSwf->setMemberByName("rentOffer", rentOffer);
						}
					}
					if (crmi->HasPricePromotion())
					{
						auto timeLeft = ConstructIAPTimeLeft(fxPlayer);
						if (timeLeft)
							itemSwf->setMemberByName(KSTR("timeLeft"), timeLeft);
					}
					

				}

				consumables->push(itemSwf);
			}
		}
	}

	// --- hc bundles next
	const std::string bundleIdBaseArr[] = { "squadmate_bundle_", "bundle_", "chain_offer_hc_", "event_chain_hc_", "daily_chain_hc_"};
	for (const auto& bundleIdBase : bundleIdBaseArr)
	{
		bool isChainStart = true;
		for (int i = 1;; ++i)
		{
			auto bundleId = bundleIdBase + std::to_string(i);
			auto bundleOffer = GetShopManager()->FindShopOffer(StringHash(bundleId.c_str()));

			if (!bundleOffer)
				break;

			// --- check for already owned item
			if (GetShopManager()->ShouldHideBundle(StringHash(bundleId.c_str())))
				continue;

			if (filter == BundleListFilter::Arena)
			{
				// --- promo more / off
				if (!IsKeyInArray(SHOP_ITEM_XTRA_ARENA_BUNDLE, bundleOffer->xtra)) //don't show if extra category contains 'price_cut'
				{
					continue;
				}
			}

			if ((filter == BundleListFilter::Daily && !IsKeyInArray(SHOP_ITEM_XTRA_DAILY_BUNDLE, bundleOffer->xtra)) ||
				(filter != BundleListFilter::Daily && IsKeyInArray(SHOP_ITEM_XTRA_DAILY_BUNDLE, bundleOffer->xtra)))
			{
				continue;
			}

			if ((filter == BundleListFilter::Events && GetBundleXtraCategoryValue(*bundleOffer, SHOP_ITEM_XTRA_EVENT_TYPE_BUNDLE).empty()) ||
				(filter != BundleListFilter::Events && !GetBundleXtraCategoryValue(*bundleOffer, SHOP_ITEM_XTRA_EVENT_TYPE_BUNDLE).empty()))
			{
				continue;
			}

			if ((filter == BundleListFilter::Rental && !IsKeyInArray(SHOP_ITEM_XTRA_RENT_BUNDLE, bundleOffer->xtra)) ||
				(filter != BundleListFilter::Rental && IsKeyInArray(SHOP_ITEM_XTRA_RENT_BUNDLE, bundleOffer->xtra)))
			{
				continue;
			}

			bool hasOfflinePromo = GetShopManager()->OfferHasPromo(*bundleOffer);

			auto itemSwf = CreateBundleObject(fxPlayer, bundleOffer, flags);
			if (itemSwf)
			{
				ret.hasOfflineBundle = true;

				// --- early out in case we just want to see if there are any items here
				if (flags & BundleListFlags::CheckExistOnly)
					return ret;

				if (bundleIdBase == "chain_offer_hc_" || bundleIdBase == "event_chain_hc_" || bundleIdBase == "daily_chain_hc_")
				{
					itemSwf->setMemberByName("category", "chain_offer_hc");
					itemSwf->setMemberByName("chainIndex", i);
					if (isChainStart)
					{
						itemSwf->setMemberByName("isChainStart", true);
						isChainStart = false;
					}
					else
					{
						itemSwf->setMemberByName("isChainStart", false);
					}
					auto timeLeft = ConstructOfflineItemsTimeLeft(fxPlayer, bundleOffer->id);
					if (timeLeft)
						itemSwf->setMemberByName(KSTR("timeLeft"), timeLeft);
				}

				if (IsKeyInArray("mystery", bundleOffer->xtra))
				{
					itemSwf->setMemberByName("mystery", true);
				}

				// rental shop bundle
				if (filter == BundleListFilter::Rental && bundleIdBase != "rent_bundle_")
				{
					itemSwf->setMemberByName("rentalShop", true);

					// --- add offer has name of this bundle's extra category
					for (int i = 1;; i++) // max 9 rent offers as GDD
					{
						auto xtra = "rent_bundle_" + std::to_string(i);
						auto xtraOffer = GetShopManager()->FindShopOffer(StringHash(xtra.c_str()));

						if (!xtraOffer)
							break;

						if (IsKeyInArray(StringHash(xtra.c_str()), bundleOffer->xtra) && xtraOffer && IsKeyInArray(SHOP_ITEM_XTRA_RENT_BUNDLE, xtraOffer->xtra))
						{
							auto rentOffer = CreateBundleObject(fxPlayer, xtraOffer, flags);
							itemSwf->setMemberByName("rentOffer", rentOffer);							
						}
					}
				} // end rent bundle 

				if (GetShopManager()->HasPromoTag(EPROMOTAG_MSG_OFFLINE, bundleOffer->id))
					itemSwf->setMemberByName(KSTR("description"), GetShopManager()->GetOfflineItemsPromoDescription(bundleOffer->id));
				if (hasOfflinePromo)
				{
					ret.hasIAPPromo = true;
					auto timeLeft = ConstructOfflineItemsTimeLeft(fxPlayer, bundleOffer->id);
					if (timeLeft)
						itemSwf->setMemberByName(KSTR("timeLeft"), timeLeft);
				}

				// --- override the title with the event title for the event bundles if an event of that type exists
				if (filter == BundleListFilter::Events)
				{
					auto eventType = StringHash(GetBundleXtraCategoryValue(*bundleOffer, SHOP_ITEM_XTRA_EVENT_TYPE_BUNDLE).c_str());
					auto it = std::find_if(eventTypeXtraMapping.begin(), eventTypeXtraMapping.end(), [eventType](const std::pair<StringHash, const char*>& entry) { return entry.first == eventType; });
					if (it != eventTypeXtraMapping.end())
					{
						const SemEvent* evt = sem_helpers::GetUniqueEvent(it->second);
						if (evt)
						{
							const auto lang = GetApplication()->GetLanguageStringISO_639_2();
							itemSwf->setMemberByName("title", ProcessUnicodeText(evt->GetName()[lang].asString().c_str()));
						}
					}
				}

				consumables->push(itemSwf);
			}
		}
	}

	// custom sort if all items have the required custom position in extra category
	bool customOrder = true;
	auto customOrderedConsumables = swfnew gameswf::ASArray(fxPlayer);
	{
		enum ECurrency { MONEY, HC, OTHER };
		enum EIcon { GOLD, SILVER, BRONZE, NONE };
		struct SortInfo
		{
			int idx;
			int order = 1024;
			float price = 0;
			ECurrency currency = OTHER;
			EIcon icon = NONE;

			explicit SortInfo(int idx) : idx(idx) {}
			SortInfo(int idx, int order) : idx(idx), order(order) {}
			SortInfo(int idx, float price, ECurrency currency, EIcon icon) : idx(idx), price(price), currency(currency), icon(icon) {}
		};
		std::vector<SortInfo> ordermap;
		for (size_t i = 0; i < consumables->size(); ++i)
		{
			auto itemSwf = consumables->get(i);
			gameswf::ASValue place;
			if ((itemSwf.toObject())->getMemberByName("customListPlace", &place))
			{
				ordermap.emplace_back(i, place.toInt());
			}
			else
			{
				gameswf::ASValue currency;
				gameswf::ASValue value;
				gameswf::ASValue internalName;
				if ((itemSwf.toObject())->getMemberByName("currency", &currency) &&
					(itemSwf.toObject())->getMemberByName("num", &value))
				{
					std::string iconstr;
					if ((itemSwf.toObject())->getMemberByName("internalName", &internalName))
					{
						iconstr = internalName.toString().c_str();
					}
					ECurrency ec = OTHER;
					std::string ecstr = currency.toString().c_str();
					if (ecstr == "money") ec = MONEY;
					else if (ecstr == "hc") ec = HC;
					EIcon icon = NONE;
					if (iconstr.find("GOLD") != std::string::npos) icon = GOLD;
					else if (iconstr.find("SILVER") != std::string::npos) icon = SILVER;
					else if (iconstr.find("BRONZE") != std::string::npos) icon = BRONZE;
					ordermap.emplace_back(i, value.toFloat(), ec, icon);
				}
				else
				{
					ordermap.emplace_back(i);
				}
			}
		}
		if (customOrder)
		{
			std::sort(ordermap.begin(), ordermap.end(), [](const SortInfo& s1, const SortInfo& s2)
				{
					if (s1.order != s2.order) return s1.order < s2.order;
					if (s1.currency != s2.currency) return s1.currency < s2.currency;
					if (s1.icon != s2.icon) return s1.icon < s2.icon;
					if (s1.currency != s2.currency) return s1.currency < s2.currency;
					return s1.price > s2.price;
				});
			for (auto it : ordermap)
			{
				auto itemSwf = consumables->get(it.idx);
				customOrderedConsumables->push(itemSwf);
			}
		}
	}

	if (customOrder)
		ret.list = customOrderedConsumables;
	else
		ret.list = consumables;

	return ret;
}