#pragma once

#include <shared_mutex>
#include <xinput.h>

#include <discord-rpc/discord_rpc.h>

constexpr auto steamAppID = "72850";

class RichPresence
{
public:
	static RichPresence* GetSingleton()
	{
		static RichPresence handler;
		return &handler;
	}

	static void InstallHooks()
	{
		Hooks::Install();
	}

	enum class Marker
	{
		None = 0,
		City = 1,
		Town = 2,
		Settlement = 3,
		Cave = 4,
		Camp = 5,
		Fort = 6,
		NordicRuin = 7,
		Dwemer = 8,
		Shipwreck = 9,
		Grove = 10,
		Landmark = 11,
		DragonLair = 12,
		Farm = 13,
		WoodMill = 14,
		Mine = 15,
		ImperialCamp = 16,
		StormcloakCamp = 17,
		Doomstone = 18,
		WheatMill = 19,
		Smelter = 20,
		Stable = 21,
		ImperialTower = 22,
		Clearing = 23,
		Pass = 24,
		Alter = 25,
		Rock = 26,
		Lighthouse = 27,
		OrcStronghold = 28,
		GiantCamp = 29,
		Shack = 30,
		NordicTower = 31,
		NordicDwelling = 32,
		Docks = 33,
		Shrine = 34,
		RiftenCastle = 35,
		RiftenCapital = 36,
		WindhelmCastle = 37,
		WindhelmCapital = 38,
		WhiterunCastle = 39,
		WhiterunCapital = 40,
		SolitudeCastle = 41,
		SolitudeCapital = 42,
		MarkarthCastle = 43,
		MarkarthCapital = 44,
		WinterholdCastle = 45,
		WinterholdCapital = 46,
		MorthalCastle = 47,
		MorthalCapital = 48,
		FalkreathCastle = 49,
		FalkreathCapital = 50,
		DawnstarCastle = 51,
		DawnstarCapital = 52,
		TempleOfMiraak = 53,
		RedoranSettlement = 54,
		AllMakerStone = 55,
		TelvanniSettlement = 56,
		TravelIconSW = 57,
		TravelIconNE = 58,
		Count,
		Unknown
	};

	std::unordered_map<void*, Marker> mapMarkerCache;

	void CacheMapMarkers();

	std::string cachedIcon;

	bool dataLoaded = false;

	RE::TESWorldSpace* MapWorldDefaultWorldSpace = nullptr;
	RE::TESObjectSTAT* Skyrim_MarkerBase = nullptr;
	RE::TESQuest* Skyrim_UnboundQuest = nullptr;

	RE::TESGlobal* Survival_ModeEnabled = nullptr;
	RE::TESGlobal* Survival_TemperatureLevel = nullptr;

	enum class UI_LEVEL
	{
		kNeutral = 0,
		kNearHeat = 1,
		kWarm = 2,
		kCold = 3,
		kFreezing = 4
	};

	bool mainMenu = true;

	void DataLoaded();

	std::string GetCurrentWorldSpaceName(std::string exclude = "");

	bool              markerUpdate = false;
	std::shared_mutex markerLock;
	std::string       markerName = "";
	std::string locationName = "";
	std::string worldSpaceName = "";
	Marker            type = Marker::None;
	float             closestDistance = FLT_MAX;
	std::string       cachedLocation;

	std::shared_mutex flavourLock;
	std::string       flavour;

	void UpdateMarker();
	void UpdateFlavour();

	std::string iconOverride = "";

	std::string applicationID = "1074109506675544146";

	std::string largeImageKey = "logo";
	std::string largeImageText = "Skyrim Special Edition";

	std::string smallImageKey = "";
	std::string smallImageText = "";

	std::string defaultExteriorIcon = "grove";
	std::string defaultInteriorIcon = "settlement";;

	bool skipUnbound = false;
	bool alwaysUpdateMarker = true;
	bool showPlayerName = false;
	bool showNotifications = false;
	float markerMinDistance = 16384;
	int messageDuration = 5;

	std::shared_mutex statsLock;
	bool showStats = true;
	int statsDuration = 2;

	std::string buttonLabel1 = "";
	std::string buttonUrl1 = "https://www.nexusmods.com/skyrimspecialedition/mods/84847";
	std::string buttonLabel2 = "";
	std::string buttonUrl2 = "";

	std::string statsButton = "";

	std::set<std::string> statNames;

	void Load();
	void Init();

	void Update();

	int messageTimer = 0;
	std::string lastMessage = "";
	void UpdateMessage(char* text);

	static bool MiscStatManager_QueryStat(RE::BSFixedString& a_stat, int& o_value)
	{
		using func_t = decltype(&MiscStatManager_QueryStat);
		REL::Relocation<func_t> func{ REL::RelocationID(16120, 16362) };  // 1.5.97 1405E1510
		return func(a_stat, o_value);
	}

	struct Hooks
	{
		struct MainUpdate_Nullsub
		{
			static void thunk()
			{
				func();
				if (GetSingleton()->dataLoaded) {
					GetSingleton()->Update();
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ShowHUDMessage_BuildHUDData
		{
			static RE::HUDData* thunk(std::uint32_t unk, char* text)
			{
				if (GetSingleton()->showNotifications) {
					GetSingleton()->UpdateMessage(text);
				}
				return func(unk, text);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct HUDNotifications_Update
		{
			static char thunk(RE::HUDNotifications* This)
			{
				if (This->queue.size())
				{
					auto& front = This->queue.front();
					if (auto text = front.text.c_str())
					{
						GetSingleton()->UpdateMessage((char*)text);
					}
				}
				return func(This);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct MiscStatManager_AddStat
		{
			static bool thunk(std::int32_t a_type, RE::BSFixedString const& a_name, char const* a_cstr, int unk1, bool unk2)
			{
				std::string name = a_name.data();
				std::lock_guard<std::shared_mutex> lockS(GetSingleton()->statsLock);
				GetSingleton()->statNames.insert(name);
				return func(a_type, a_name, a_cstr, unk1, unk2);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<MainUpdate_Nullsub>(REL::RelocationID(35565, 36564).address() + REL::Relocate(0x748, 0xC26));
			stl::write_thunk_call<ShowHUDMessage_BuildHUDData>(REL::RelocationID(52050, 52933).address() + REL::Relocate(0x19B, 0x31D));
			stl::write_vfunc<0x1, HUDNotifications_Update>(RE::VTABLE_HUDNotifications[0]);
			stl::write_thunk_call<MiscStatManager_AddStat>(REL::RelocationID(16121, 16363).address() + 0x12);
		}
	};

private:
	RichPresence()
	{
		Load();
	}

	RichPresence(const RichPresence&) = delete;
	RichPresence(RichPresence&&) = delete;

	~RichPresence() = default;

	RichPresence& operator=(const RichPresence&) = delete;
	RichPresence& operator=(RichPresence&&) = delete;
};
