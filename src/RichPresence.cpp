#include "RichPresence.h"

#include <SimpleIni.h>
#include <magic_enum.hpp>

void RichPresence::Load()
{
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(L"Data\\SKSE\\Plugins\\RichPresence.ini");

	applicationID = ini.GetValue("Settings", "ApplicationID", "1074109506675544146");

	largeImageKey = ini.GetValue("Settings", "LargeImageKey", "logo");
	largeImageText = ini.GetValue("Settings", "LargeImageText", "The Elder Scrolls V: Skyrim");

	smallImageKey = ini.GetValue("Settings", "SmallImageKey", "");
	smallImageText = ini.GetValue("Settings", "SmallImageText", "");
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

void RichPresence::DataLoaded()
{
	markerBase = RE::TESForm::LookupByID(0x10)->As<RE::TESObjectSTAT>();
	auto sMapWorldDefaultWorldSpace = RE::GetINISetting("sMapWorldDefaultWorldSpace:MapMenu");
	defaultWorldSpace = RE::TESForm::LookupByEditorID(sMapWorldDefaultWorldSpace->GetString());
	dataLoaded = true;
}

const char* RichPresence::GetCurrentWorldSpaceName()
{
	if (auto tes = RE::TES::GetSingleton()) {
		auto cached = RE::TES::GetSingleton()->worldSpace;
		if (!cached) {
			cached = defaultWorldSpace->As<RE::TESWorldSpace>();
		}
		for (auto worldSpace = cached; worldSpace; worldSpace = worldSpace->parentWorld) {
			auto worldSpaceName = worldSpace->GetName();
			if (worldSpaceName) {
				return worldSpaceName;
			}
		}
	}
	return nullptr;
}

void RichPresence::UpdateMarker()
{
	auto startTime = std::chrono::system_clock::now();
	std::lock_guard<std::shared_mutex> lk(markerLock);
	type = Marker::None;
	closestDistance = FLT_MAX;
	markerName = "";
	locationName = "";
	RE::TESWorldSpace* cached = nullptr;
	if (auto player = RE::PlayerCharacter::GetSingleton()) {
		cached = player->GetWorldspace();
		cached = player->GetPlayerRuntimeData().cachedWorldSpace;

		if (!cached) {
			cached = defaultWorldSpace->As<RE::TESWorldSpace>();
		}

		auto location = player->GetCurrentLocation();
		while (location) {
			if (type == Marker::None) {
				if (auto handle = location->worldLocMarker) {
					if (auto ptr = handle.get()) {
						if (auto ref = ptr.get()) {
							if (auto marker = ref->extraList.GetByType<RE::ExtraMapMarker>()) {
								type = (Marker)marker->mapData->type.underlying();
								markerName = marker->mapData->locationName.fullName;
							}
						}
					}
				}
			}
			if (locationName.empty())
			{
				locationName = location->GetName();
			}
			if (location != location->parentLoc) {
				location = location->parentLoc;
			}
			else {
				location = nullptr;
			}
		}

		if (type == Marker::None) {
			bool interior = PlayerIsInInterior();
			auto worldspace = cached;
			auto position = interior ? player->GetPlayerRuntimeData().exteriorPosition : player->GetPosition();
			while (worldspace) {
				worldspace->persistentCell->ForEachReferenceInRange(position, markerMinDistance, [&](RE::TESObjectREFR& a_ref) {
					if (a_ref.GetBaseObject() == markerBase) {
						if (auto marker = a_ref.extraList.GetByType<RE::ExtraMapMarker>()) {
							auto distance = a_ref.GetPosition().GetDistance(player->GetPosition());
							if (distance < closestDistance) {
								closestDistance = distance;
								type = (Marker)marker->mapData->type.underlying();
								markerName = marker->mapData->locationName.fullName;
							}
						}
					}
					return RE::BSContainer::ForEachResult::kContinue;
					});
				if (worldspace != worldspace->parentWorld) {
					worldspace = worldspace->parentWorld;
				}
				else {
					worldspace = nullptr;
				}
			}
		}
	}

	if (type != Marker::None) {
		cachedIcon = magic_enum::enum_name(type).data();
		std::transform(cachedIcon.begin(), cachedIcon.end(), cachedIcon.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
	}

	if (!locationName.empty())
	{
		cachedLocation = locationName;
		inLocation = PlayerIsInInterior() || (cached && cached->parentWorld);
	}
	else if (!markerName.empty())
	{
		cachedLocation = markerName;
		inLocation = false;
	}
	else {
		cachedLocation = GetCurrentWorldSpaceName();
		inLocation = true;
	}

	auto endTime = std::chrono::system_clock::now();
	logger::info("Marker update took {} microseconds", std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
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
		if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::TESObjectREFR::LookupByHandle(RE::BarterMenu::GetTargetRefHandle())) {
				if (auto name = ref->GetName()) {
					flavour += std::format("Bartering with {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour += "Bartering";
			}
		}
		else if (ui->IsMenuOpen(RE::BookMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::BookMenu::GetTargetForm()) {
				if (auto name = ref->GetName()) {
					flavour += std::format("Reading {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour += "Reading";
			}
		}
		else if (ui->IsMenuOpen(RE::Console::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::Console::GetSelectedRef()) {
				if (auto name = ref->GetName()) {
					flavour += std::format("In the console ({})", name);
					named = true;
				}
				else {
					flavour += std::format("In the console ({:X})", ref->GetFormID());
				}
				named = true;
			}
			if (!named) {
				flavour += "In the console";
			}
		}
		else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::TESObjectREFR::LookupByHandle(RE::ContainerMenu::GetTargetRefHandle())) {
				if (auto name = ref->GetName()) {
					if (auto actor = ref->As<RE::Actor>()) {
						flavour += std::format("Searching {} {}", actor->GetActorBase()->IsUnique() ? "" : " a ", name);
					}
					else {
						flavour += std::format("Searching {}", name);
					}
					named = true;
				}
			}
			if (!named) {
				flavour += "Searching a container";
			}
		}
		else if (ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME)) {
			bool named = false;
			if (auto gptr = ui->GetMenu<RE::CraftingMenu>()) {
				auto menu = (RE::CraftingMenu*)gptr.get();
				if (auto sub = menu->GetCraftingSubMenu()) {
					if (auto furn = sub->furniture) {
						if (auto name = furn->GetName()) {
							flavour += std::format("Using {}", name);
							named = true;
						}
					}
				}
			}
			if (!named) {
				flavour += "Crafting";
			}
		}
		else if (ui->IsMenuOpen(RE::CreationClubMenu::MENU_NAME)) {
			flavour += "Looking at Creation Club";
		}
		else if (ui->IsMenuOpen(RE::CreditsMenu::MENU_NAME)) {
			flavour += "Watching credits";
		}
		else if (ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME)) {
			flavour += "Equipping items";
		}
		else if (ui->IsMenuOpen(RE::GiftMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::TESObjectREFR::LookupByHandle(RE::GiftMenu::GetTargetRefHandle())) {
				if (auto name = ref->GetName()) {
					flavour += std::format("Giving a gift to {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour += "Giving a gift";
			}
		}
		else if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
			flavour += "In the inventory";
		}
		else if (ui->IsMenuOpen(RE::JournalMenu::MENU_NAME)) {
			flavour += "Looking at the journal";
		}
		else if (ui->IsMenuOpen(RE::LevelUpMenu::MENU_NAME)) {
			flavour += "Advancing an attribute";
		}
		else if (ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
			flavour += "Loading";
			iconOverride = "loading";
		}
		else if (ui->IsMenuOpen(RE::LoadWaitSpinner::MENU_NAME)) {
			flavour += "Loading";
			iconOverride = "loading";
		}
		else if (ui->IsMenuOpen(RE::LockpickingMenu::MENU_NAME)) {
			bool named = false;
			if (auto ref = RE::LockpickingMenu::GetTargetReference()) {
				if (auto name = ref->GetName()) {
					flavour += std::format("Lockpicking {}", name);
					named = true;
				}
			}
			if (!named) {
				flavour += "Lockpicking";
			}
			iconOverride = "locked";
		}
		else if (ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
			flavour += "In the magic menu";
		}
		else if (ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) {
			flavour += "On the main menu";
		}
		else if (ui->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
			flavour += "Looking at the map";
		}
		else if (ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME)) {
			flavour += "Reading a message";
		}
		else if (ui->IsMenuOpen(RE::MistMenu::MENU_NAME)) {
			flavour += "Loading";
			iconOverride = "loading";
		}
		else if (ui->IsMenuOpen(RE::ModManagerMenu::MENU_NAME)) {
			flavour += "Managing mods";
		}
		else if (ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
			flavour += "Editing character";
		}
		else if (ui->IsMenuOpen(RE::SleepWaitMenu::MENU_NAME)) {
			flavour += "Waiting";
		}
		else if (ui->IsMenuOpen(RE::StatsMenu::MENU_NAME)) {
			flavour += "Looking at skills";
		}
		else if (ui->IsMenuOpen(RE::TitleSequenceMenu::MENU_NAME)) {
			flavour += "On the main menu";
		}
		else if (ui->IsMenuOpen(RE::TutorialMenu::MENU_NAME)) {
			flavour += "Reading a tutorial";
		}
		else if (ui->IsMenuOpen(RE::TweenMenu::MENU_NAME)) {
			flavour += "In a menu";
		}
		else if (speaker) {
			if (auto name = speaker->GetName()) {
				if (ui->IsMenuOpen(RE::TrainingMenu::MENU_NAME)) {
					flavour += std::format("Training with {}", name);
				}
				else {
					flavour += std::format("Talking to {}", name);
				}
			}
			else {
				flavour += "Talking to someone";
			}
		}
	}

	if (!RE::UI::GetSingleton()->GameIsPaused()) {
		if (auto player = RE::PlayerCharacter::GetSingleton()) {
			if (flavour.empty()) {
				bool         mounted = false;
				RE::ActorPtr ptr;
				if (auto mount = player->GetMount(ptr) && ptr) {
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
					else {
						if (auto process = player->GetActorRuntimeData().currentProcess)
						{
							if (auto target = process->GetHeadtrackTarget())
							{
								bool named = false;
								if (auto ref = target.get()) {
									if (ref.get() == player) {
									}
									else if (auto name = ref->GetName()) {
										flavour = std::format("Looking at {}", name);
									}
									named = true;
								}
								if (!named)
								{
									flavour = "Looking at something";
								}
							}
						}
					}
				}
			}

			std::lock_guard<std::shared_mutex> lockM(markerLock);
			if (!cachedLocation.empty()) {
				if (inLocation) {
					if (!flavour.empty()) {
						flavour += std::format(" in {}", cachedLocation);
					}
					else {
						flavour += std::format("In {} within {}", cachedLocation, GetCurrentWorldSpaceName());
					}
				}
				else {
					if (closestDistance > markerMinDistanceHalf) {
						if (!flavour.empty()) {
							flavour += std::format(" near {}", cachedLocation);
						}
						else {
							flavour += std::format("Near {} within {}", cachedLocation, GetCurrentWorldSpaceName());
						}
					}
					else {
						if (!flavour.empty()) {
							flavour += std::format(" at {}", cachedLocation);
						}
						else {
							flavour += std::format("At {} within {}", cachedLocation, GetCurrentWorldSpaceName());
						}
					}
				}
			}
		}
	}
}

void RichPresence::Init()
{
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
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
					details += std::format("{} - {} ({})", player->GetName(), raceName, player->GetLevel());

					static void* cellPtr = nullptr;
					auto currentCell = player->GetParentCell();;
					if (cellPtr != currentCell)
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
			discord_presence.details = details.c_str();
			static auto timer = time(nullptr);
			discord_presence.startTimestamp = timer;

			if (type != Marker::None && type < Marker::Count) {
				discord_presence.largeImageKey = cachedIcon.c_str();
			}
			else {
				discord_presence.largeImageKey = largeImageKey.c_str();
			}

			discord_presence.largeImageText = largeImageText.c_str();
			discord_presence.smallImageKey = smallImageKey.c_str();
			discord_presence.smallImageText = smallImageText.c_str();

			if (!iconOverride.empty()) {
				discord_presence.smallImageKey = iconOverride.c_str();
			}

			Discord_UpdatePresence(&discord_presence);
			Discord_RunCallbacks();
		}
	}
	else {
		static float& g_deltaTimeRealTime = (*(float*)RELOCATION_ID(523661, 410200).address());                 // 2F6B94C, 30064CC
		delay -= g_deltaTimeRealTime;
	}
}