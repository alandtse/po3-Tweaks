#include "Experimental.h"
#include "Fixes.h"
#include "Tweaks.h"
#include "Settings.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		{	
			SpellNoAbsorb::Install();
			
			auto& tweaks = Settings::GetSingleton()->tweaks;
			if (tweaks.grabbingIsStealing.value) {
				GrabbingIsStealing::Install();
			}
		}
		break;
	default:
		break;
	}
}

extern "C" __declspec(dllexport) constexpr auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v{};
	v.pluginVersion = Version::MAJOR;
	v.PluginName(Version::PROJECT.data());
	v.AuthorName("powerofthree"sv);
	v.CompatibleVersions({ SKSE::RUNTIME_1_6_318 });
	return v;
}();

bool InitLogger()
{
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	return true;
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	logger::info("loaded plugin");

	if (!InitLogger()) {
		return false;
	}

	SKSE::Init(a_skse);

	auto trampolineSize = Settings::GetSingleton()->Load();
	SKSE::AllocTrampoline(trampolineSize * 14);

	logger::info("{:*^30}", "PATCH START"sv);
	
	Fixes::Install();
	Tweaks::Install();
	Experimental::Install();

	logger::info("{:*^30}", "PATCHES FINISH"sv);

	auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
