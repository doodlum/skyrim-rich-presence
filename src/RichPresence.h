#pragma once

#include <shared_mutex>
#include <xinput.h>

#include <discord-rpc/discord_rpc.h>

constexpr auto steamAppID = "72850";
constexpr auto markerMinDistance = 16384;
constexpr auto markerMinDistanceHalf = 4096;

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
		DwemerRuin = 8,
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
		Count
	};

	std::string cachedIcon;

	bool dataLoaded = false;
	RE::TESObjectSTAT* markerBase = nullptr;
	RE::TESForm* defaultWorldSpace = nullptr;
	RE::TESQuest* unboundQuest = nullptr;

	bool mainMenu = true;

	void DataLoaded();

	bool              markerUpdate = false;
	std::shared_mutex markerLock;
	std::string       markerName = "";
	std::string locationName = "";
	Marker            type = Marker::None;
	float             closestDistance = FLT_MAX;
	std::string       cachedLocation;
	bool              inLocation = true;

	std::shared_mutex flavourLock;
	std::string       flavour;

	const char* GetCurrentWorldSpaceName(std::string exclude = "");

	void UpdateMarker();
	void UpdateFlavour();

	std::string iconOverride = "";

	std::string applicationID = "1074109506675544146";

	std::string largeImageKey = "logo";
	std::string largeImageText = "Skyrim Special Edition";

	std::string smallImageKey = "";
	std::string smallImageText = "";

	bool skipUnbound = false;

	float delay = 0;

	void Load();
	void Init();

	void Update();

	struct Hooks
	{
		struct MainUpdate_Nullsub
		{
			static void thunk()
			{
				func();
				GetSingleton()->Update();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_thunk_call<MainUpdate_Nullsub>(REL::RelocationID(35565, 36564).address() + REL::Relocate(0x748, 0xC26));
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
