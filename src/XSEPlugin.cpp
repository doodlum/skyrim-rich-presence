#include "RichPresence.h"

static void MessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
	{
		RichPresence::GetSingleton()->Init();
		RichPresence::GetSingleton()->DataLoaded();
		break;
	}
	}
}

void Load()
{
	auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener("SKSE", MessageHandler);
	RichPresence::InstallHooks();
}