#include "RichPresence.h"

#include <SimpleIni.h>
#include <magic_enum.hpp>

void RichPresence::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(L"Data\\SKSE\\Plugins\\DiscordRichPresence.ini");

	applicationID = ini.GetValue("Application", "ApplicationID", "1074109506675544146");

	largeImageKey = ini.GetValue("Default", "LargeImageKey", "legendary");
	largeImageText = ini.GetValue("Default", "LargeImageText", "Skyrim Special Edition");

	smallImageKey = ini.GetValue("Default", "SmallImageKey", "");
	smallImageText = ini.GetValue("Default", "SmallImageText", "");

	defaultExteriorIcon = ini.GetValue("Default", "DefaultExteriorIcon", "grove");
	defaultInteriorIcon = ini.GetValue("Default", "DefaultInteriorIcon", "settlement");

	skipUnbound = ini.GetBoolValue("Options", "SkipUnbound", false);
	alwaysUpdateMarker = ini.GetBoolValue("Options", "AlwaysUpdateMarker", true);
}

bool PlayerIsInInterior()
{
	if (auto player = RE::PlayerCharacter::GetSingleton()) {
		if (auto parentCell = player->parentCell) {
			return parentCell->IsInteriorCell();
		}
	}
	return false;
}

void RichPresence::CacheMapMarkers()
{
	auto  dataHandler = RE::TESDataHandler::GetSingleton();
	auto& worldSpaceArray = dataHandler->GetFormArray<RE::TESWorldSpace>();
	for (auto& worldSpace : worldSpaceArray)
	{
		if (auto name = worldSpace->GetName()) {
			logger::debug("World Space {}", name);
		}
		else {
			logger::debug("World Space Unknown");
		}
		std::list<RE::TESObjectREFR*> mapMarkers;
		if (worldSpace->persistentCell) {
			worldSpace->persistentCell->ForEachReference([&](RE::TESObjectREFR& a_ref) {
				if (auto marker = a_ref.extraList.GetByType<RE::ExtraMapMarker>()) {
					if (auto name = marker->mapData->locationName.GetFullName()) {
						logger::debug("Marker {} {}", marker->mapData->type.underlying(), name);
					}
					else {
						logger::debug("Marker {} Unknown", marker->mapData->type.underlying());
					}
					if (marker->mapData->type.underlying() < 59) {
						mapMarkerCache.insert({ &a_ref, (Marker)marker->mapData->type.underlying() });
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
				});
		}
	}
}

void RichPresence::DataLoaded()
{
	auto sMapWorldDefaultWorldSpace = RE::GetINISetting("sMapWorldDefaultWorldSpace:MapMenu");
	MapWorldDefaultWorldSpace = RE::TESForm::LookupByEditorID(sMapWorldDefaultWorldSpace->GetString())->As<RE::TESWorldSpace>();
	Skyrim_MarkerBase = RE::TESForm::LookupByID(0x10)->As<RE::TESObjectSTAT>();
	Skyrim_UnboundQuest = RE::TESForm::LookupByEditorID("MQ101")->As<RE::TESQuest>();
	auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (dataHandler->LookupLoadedLightModByName("ccQDRSSE001-SurvivalMode.esl")) {
		Survival_ModeEnabled = dataHandler->LookupForm(0x2EDD, "Update.esm")->As<RE::TESGlobal>();
		Survival_TemperatureLevel = dataHandler->LookupForm(0x826, "ccQDRSSE001-SurvivalMode.esl")->As<RE::TESGlobal>();
	}
	CacheMapMarkers();
	dataLoaded = true;
}

std::string RichPresence::GetCurrentWorldSpaceName(std::string exclude)
{
	if (auto player = RE::PlayerCharacter::GetSingleton()) {
		auto cached = player->GetPlayerRuntimeData().cachedWorldSpace;
		if (!cached) {
			cached = MapWorldDefaultWorldSpace->As<RE::TESWorldSpace>();
		}
		for (auto worldSpace = cached; worldSpace; worldSpace = worldSpace->parentWorld) {
			auto worldSpaceNameTemp = worldSpace->GetName();
			if (worldSpaceNameTemp && strcmp(worldSpaceNameTemp, exclude.c_str()) != 0) {
				return worldSpaceNameTemp;
			}
		}
	}
	return std::string();
}

void RichPresence::UpdateMarker()
{
	std::lock_guard<std::shared_mutex> lk(markerLock);
	type = Marker::None;
	closestDistance = FLT_MAX;
	markerName = "";
	locationName = "";
	worldSpaceName = GetCurrentWorldSpaceName();
	RE::TESWorldSpace* cached = nullptr;
	bool interior = PlayerIsInInterior();
	if (auto player = RE::PlayerCharacter::GetSingleton()) {
		cached = player->GetPlayerRuntimeData().cachedWorldSpace;

		if (!cached) {
			cached = MapWorldDefaultWorldSpace->As<RE::TESWorldSpace>();
		}

		for (auto location = player->GetCurrentLocation(); location && locationName.empty(); location = location->parentLoc) {
			if (auto name = location->GetName()) {
				locationName = name;
			}
		}

		auto position = interior ? player->GetPlayerRuntimeData().exteriorPosition : player->GetPosition();
		for (auto worldSpace = cached; worldSpace; worldSpace = worldSpace->parentWorld) {
			worldSpace->persistentCell->ForEachReferenceInRange(position, markerMinDistance, [&](RE::TESObjectREFR& a_ref) {
				if (a_ref.GetBaseObject() == Skyrim_MarkerBase) {
					if (auto marker = a_ref.extraList.GetByType<RE::ExtraMapMarker>()) {
						auto distance = a_ref.GetPosition().GetDistance(position);
						if (auto name = marker->mapData->locationName.fullName.c_str()) {
							if (strcmp(name, locationName.c_str()) == 0) {
								closestDistance = 0;
								if (marker->mapData->type.underlying() < 59) {
									type = (Marker)marker->mapData->type.underlying();
								}
								else {
									auto it = mapMarkerCache.find(&a_ref);
									if (it != mapMarkerCache.end()) {
										type = (*it).second;
									}
									else {
										type = Marker::Unknown;
									}
								}
								markerName = name;
							}
							else if (distance < closestDistance && marker->mapData->flags.all(RE::MapMarkerData::Flag::kVisible)) {
								closestDistance = distance;
								if (marker->mapData->type.underlying() < 59) {
									type = (Marker)marker->mapData->type.underlying();
								}
								else {
									auto it = mapMarkerCache.find(&a_ref);
									if (it != mapMarkerCache.end()) {
										type = (*it).second;
									}
									else {
										type = Marker::Unknown;
									}
								}
								markerName = name;
							}
						}
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
				});
		}
	}

	if (type != Marker::None) {
		cachedIcon = magic_enum::enum_name(type).data();
		std::transform(cachedIcon.begin(), cachedIcon.end(), cachedIcon.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
	}
	else if (auto player = RE::PlayerCharacter::GetSingleton())
	{
		if (auto cell = player->GetParentCell())
		{
			if (cell->IsInteriorCell())
			{
				cachedIcon = defaultInteriorIcon;
			}
			else {
				cachedIcon = defaultExteriorIcon;
			}
		}
	}

	if (!locationName.empty())
	{
		cachedLocation = locationName;
	}
	else if (!markerName.empty())
	{
		cachedLocation = markerName;
	}
	else {
		cachedLocation = worldSpaceName;
	}

	worldSpaceName = GetCurrentWorldSpaceName(cachedLocation);
}

void RichPresence::UpdateFlavour()
{
	std::lock_guard<std::shared_mutex> lockF(flavourLock);
	flavour = "";
	iconOverride = "";

	RE::TESObjectREFR* speaker;
	if (RE::MenuTopicManager::GetSingleton()->speaker) {
		speaker = RE::MenuTopicManager::GetSingleton()->speaker.get().get();
	}
	else {
		speaker = RE::MenuTopicManager::GetSingleton()->lastSpeaker.get().get();
	}

	if (auto ui = RE::UI::GetSingleton()) {
		mainMenu = false;
		if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::TESObjectREFR::LookupByHandle(RE::BarterMenu::GetTargetRefHandle())) {
				if (auto name = ref->GetName()) {
					flavour = std::format("Bartering with {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour = "Bartering";
			}
		}
		else if (ui->IsMenuOpen(RE::BookMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::BookMenu::GetTargetForm()) {
				if (auto name = ref->GetName()) {
					flavour = std::format("Reading {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour = "Reading";
			}
		}
		else if (ui->IsMenuOpen(RE::Console::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::Console::GetSelectedRef()) {
				if (auto name = ref->GetName()) {
					flavour = std::format("Console ({})", name);
					named = true;
				}
				else {
					flavour = std::format("Console ({:X})", ref->GetFormID());
				}
				named = true;
			}
			if (!named) {
				flavour = "Console";
			}
		}
		else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle())) {
				if (auto name = ref->GetName()) {
					if (auto actor = ref->As<RE::Actor>()) {
						flavour = std::format("Searching {}", name);
					}
					else {
						flavour = std::format("Searching {}", name);
					}
					named = true;
				}
			}
			if (!named) {
				flavour = "Searching container";
			}
		}
		else if (ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) {
			bool named = false;
			if (auto gptr = ui->GetMenu<RE::CraftingMenu>()) {
				auto menu = (RE::CraftingMenu*)gptr.get();
				if (auto sub = menu->GetCraftingSubMenu()) {
					if (auto furn = sub->furniture) {
						if (auto name = furn->GetName()) {
							flavour = std::format("Using {}", name);
							named = true;
						}
					}
				}
			}
			if (!named) {
				flavour = "Crafting";
			}
		}
		else if (ui->IsMenuOpen(RE::CreationClubMenu::MENU_NAME)) {
			flavour = "Creation Club Menu";
		}
		else if (ui->IsMenuOpen(RE::CreditsMenu::MENU_NAME)) {
			flavour = "Watching credits";
		}
		else if (ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) {
			flavour = "Favourites Menu";
		}
		else if (ui->IsMenuOpen(RE::GiftMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::TESObjectREFR::LookupByHandle(RE::GiftMenu::GetTargetRefHandle())) {
				if (auto name = ref->GetName()) {
					flavour = std::format("Giving gift to {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour = "Giving gift";
			}
		}
		else if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
			flavour = "Inventory Menu";
		}
		else if (ui->IsMenuOpen(RE::ModManagerMenu::MENU_NAME)) {
			flavour = "Mod Manager Menu";
		}
		else if (ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
			flavour = "Journal Menu";
		}
		else if (ui->IsMenuOpen(RE::LevelUpMenu::MENU_NAME)) {
			flavour = "LevelUp Menu";
		}
		else if (ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::LockpickingMenu::GetTargetReference()) {
				std::string level = "Unlocked";
				switch (ref->GetLockLevel())
				{
				case RE::LOCK_LEVEL::kVeryEasy:
					level = "Very Easy";
					break;
				case RE::LOCK_LEVEL::kEasy:
					level = "Easy";
					break;
				case RE::LOCK_LEVEL::kAverage:
					level = "Average";
					break;
				case RE::LOCK_LEVEL::kHard:
					level = "Hard";
					break;
				case RE::LOCK_LEVEL::kVeryHard:
					level = "Very Hard";
					break;
				case RE::LOCK_LEVEL::kRequiresKey:
					level = "Requires Key";
					break;
				}
				if (auto name = ref->GetName()) {
					flavour = std::format("Lockpicking {} ({})", name, level);
					named = true;
				}
			}
			if (!named) {
				flavour = "Lockpicking";
			}
			iconOverride = "locked";
		}
		else if (ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
			flavour = "Magic Menu";
		}
		else if (ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
			flavour = "Main Menu";
			mainMenu = true;
		}
		else if (ui->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
			flavour = "Map Menu";
		}
		else if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME)) {
			flavour = "Message Box";
		}
		else if (ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
			flavour = "Race Menu";
		}
		else if (ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME)) {
			flavour = "Sleep/Wait Menu";
		}
		else if (ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)) {
			flavour = "Stats Menu";
		}
		else if (ui->IsMenuOpen(RE::TitleSequenceMenu::MENU_NAME)) {
			flavour = "New Game";
			mainMenu = true;
		}
		else if (ui->IsMenuOpen(RE::TutorialMenu::MENU_NAME)) {
			flavour = "Tutorial Menu";
		}
		else if (ui->IsMenuOpen(RE::TweenMenu::MENU_NAME)) {
			flavour = "Tween Menu";
		}
		else if (ui->IsMenuOpen(RE::TrainingMenu::MENU_NAME)) {
			bool named = false;
			if (speaker) {
				if (auto name = speaker->GetName()) {
					flavour = std::format("Training with {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour = "Training";
			}
		}
	}

	if (!flavour.empty() && iconOverride.empty())
	{
		iconOverride = "checkmark";
	}

	if (iconOverride.empty())
	{
		if (Survival_ModeEnabled && Survival_ModeEnabled->value && Survival_TemperatureLevel && Survival_TemperatureLevel->value)
		{
			auto value = Survival_TemperatureLevel->value;
			if (value > 2)
			{
				iconOverride = "survivalwarm";
			}
			else {
				iconOverride = "survivalcold";
			}
		}
	}

	if (flavour.empty()) {
		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			bool         mounted = false;
			RE::ActorPtr ptr;
			if (speaker) {
				if (auto name = speaker->GetName()) {
					flavour = std::format("Talking to {}", name);
				}
				else {
					flavour = "Talking to someone";
				}
			}
			else if (auto mount = player->GetMount(ptr) && ptr) {
				mounted = true;
				if (auto name = ptr->GetName()) {
					flavour = std::format("Riding {}", name);
				}
				else {
					flavour = std::format("Riding", name);
				}
			}

			if (!mounted) {
				if (player->IsDead()) {
					flavour = "Died";
				}
				else if (player->IsInRagdollState()) {
					flavour = "Ragdolling";
				}
				else if (player->IsInCombat()) {
					bool named = false;
					if (auto target = player->GetActorRuntimeData().currentCombatTarget) {
						if (auto ref = target.get()) {
							if (auto name = ref->GetName()) {
								flavour = std::format("Fighting {}", name);
								named = true;
							}
						}
					}
					if (!named)
					{
						flavour = "Fighting";
					}
					iconOverride = "enemyclose";
				}
				else if (player->IsSneaking()) {
					flavour = "Sneaking";
				}
				else if (player->AsActorState()->IsFlying()) {
					flavour = "Flying";
				}
				else if (player->AsActorState()->IsSwimming()) {
					flavour = "Swimming";
				}
				else if (auto handle = player->GetOccupiedFurniture())
				{
					if (auto ref = handle.get()) {
						bool named = false;
						if (auto name = ref->GetName()) {
							flavour = std::format("Using {}", name);
							named = true;
						}
						if (!named)
						{
							flavour = "Using object";
						}
					}
				}
			}

			std::lock_guard<std::shared_mutex> lockM(markerLock);
			if (!cachedLocation.empty()) {
				std::string range = "";
				if (!flavour.empty()) {
					flavour += std::format(", {}", cachedLocation);
				}
				else if (!worldSpaceName.empty()) {
					flavour += std::format("{}, {}", cachedLocation, worldSpaceName);
				}
				else {
					flavour = std::format("{}", cachedLocation);
				}
			}
		}
	}
}

void HandleDiscordReady(const DiscordUser* connected_user)
{
	logger::info("Discord RPC: connected to user {}{} - {}", connected_user->username, connected_user->discriminator, connected_user->userId);
}

void HandleDiscordError(const int error_code, const char* message)
{
	logger::info("Discord RPC: an error occured ({}: {})", error_code, message);
}

void HandleDiscordDisconnected(const int error_code, const char* message)
{
	logger::info("Discord RPC: disconnected ({}: {})", error_code, message);
}

void RichPresence::Init()
{
	DiscordEventHandlers handlers;
	ZeroMemory(&handlers, sizeof(handlers));
	handlers.ready = HandleDiscordReady;
	handlers.disconnected = HandleDiscordError;
	handlers.disconnected = HandleDiscordDisconnected;
	Discord_Initialize(applicationID.c_str(), &handlers, 1, steamAppID);
	logger::info("Discord RPC Initialised");
}

void RichPresence::Update()
{
	if (delay <= 0) {
		std::unique_lock<std::shared_mutex> lockF(flavourLock, std::try_to_lock);
		std::unique_lock<std::shared_mutex> lockM(markerLock, std::try_to_lock);
		if (lockF.owns_lock() && lockM.owns_lock()) {
			delay = 1.0f;

			std::string details = "";

			if (dataLoaded) {
				if (auto player = RE::PlayerCharacter::GetSingleton()) {
					std::string raceName = "";
					if (auto race = player->GetRace()) {
						raceName = race->GetName();
					}
					details += std::format("{}, {}, Level {}", player->GetName(), raceName, player->GetLevel());

					static void* cellPtr = nullptr;
					auto currentCell = player->GetParentCell();;
					if (alwaysUpdateMarker || (cellPtr != currentCell))
					{
						cellPtr = currentCell;
						SKSE::GetTaskInterface()->AddTask([&]() {
							UpdateMarker();
							});
					}
				}
			}

			SKSE::GetTaskInterface()->AddUITask([&]() {
				UpdateFlavour();
				});

			DiscordRichPresence discord_presence;
			memset(&discord_presence, 0, sizeof(discord_presence));
			discord_presence.state = flavour.c_str();
			if (!mainMenu && dataLoaded && (skipUnbound || Skyrim_UnboundQuest->IsCompleted())) {
				discord_presence.details = details.c_str();
			}

			static auto timer = time(nullptr);
			discord_presence.startTimestamp = timer;

			if (!cachedIcon.empty() && !mainMenu) {
				discord_presence.largeImageKey = cachedIcon.c_str();
			}
			else {
				discord_presence.largeImageKey = largeImageKey.c_str();
			}

			discord_presence.largeImageText = largeImageText.c_str();
			discord_presence.smallImageKey = smallImageKey.c_str();
			discord_presence.smallImageText = smallImageText.c_str();

			if (messageTimer > 0 && !lastMessage.empty())
			{
				discord_presence.details = discord_presence.state;
				discord_presence.state = lastMessage.c_str();
				messageTimer -= 1;
			}

			if (!iconOverride.empty()) {
				discord_presence.smallImageKey = iconOverride.c_str();
			}

			Discord_UpdatePresence(&discord_presence);
			Discord_RunCallbacks();
		}
	}
	else {
		static float& g_deltaTimeRealTime = (*(float*)RELOCATION_ID(523661, 410200).address()); // 2F6B94C, 30064CC
		delay -= g_deltaTimeRealTime;
	}
}

void RichPresence::UpdateMessage(char* text)
{
	std::lock_guard<std::shared_mutex> lockF(flavourLock);
	if (text) {
		messageTimer = messageDuration;
		lastMessage = text;
		if (lastMessage.back() != '.')
		{
			lastMessage += '.';
		}
	}
}