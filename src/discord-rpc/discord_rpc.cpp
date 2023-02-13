#pragma once

#include "discord_rpc.h"

namespace Discord
{
	static SKSE::WinAPI::HMODULE hModule = (SKSE::WinAPI::HMODULE)LoadLibrary(L"Data\\SKSE\\Plugins\\discord-rpc\\discord-rpc.dll");
}

typedef const char* (*_Discord_Initialize)(const char* applicationId,
	DiscordEventHandlers*                              handlers,
	int                                                autoRegister,
	const char*                                        optionalSteamId);

const char* Discord_Initialize(const char* applicationId,
	DiscordEventHandlers*                  handlers,
	int                                    autoRegister,
	const char*                            optionalSteamId)
{
	return reinterpret_cast<_Discord_Initialize>(WinAPI::GetProcAddress(Discord::hModule, "Discord_Initialize"))(applicationId, handlers, autoRegister, optionalSteamId);
}

/* checks for incoming messages, dispatches callbacks */
typedef void (*_Discord_RunCallbacks)(void);
const void Discord_RunCallbacks(void)
{
	return reinterpret_cast<_Discord_RunCallbacks>(WinAPI::GetProcAddress(Discord::hModule, "Discord_RunCallbacks"))();
}

typedef void (*_Discord_UpdatePresence)(const DiscordRichPresence* presence);
const void Discord_UpdatePresence(const DiscordRichPresence* presence)
{
	return reinterpret_cast<_Discord_UpdatePresence>(WinAPI::GetProcAddress(Discord::hModule, "Discord_UpdatePresence"))(presence);
}
