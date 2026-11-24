#include "Arousal.h"
#include "Devious.h"
#include "Heat.h"
#include "Manager.h"
#include "Mdnm.h"
#include "Menu.h"
#include "OStim.h"
#include "Papyrus.h"
#include "Settings.h"
#include "Skee.h"
#include "Status.h"
#include "Strings.h"
#include "Actors.h"
#include "Visuals.h"
#include "Widget.h"
#include "drip/Drips.h"

namespace
{
	// both would paint the same skin: WetFunction NG through RaceMenu overrides, Drench straight into the materials
	bool        g_predecessorLoaded = false;

	void InitializeLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= "Drench.log";

		auto log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		using namespace Drench;
		switch (a_message->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
			// RaceMenu is optional: it is only asked to drop what WetFunction left in old saves
			Skee::Connect();
			Strings::Load();
			Menu::Register();
			Widget::Register();
			g_predecessorLoaded = ::GetModuleHandleW(L"WetFunctionNG.dll") != nullptr;
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			if (!Skee::Overrides()) {
				Skee::ConnectLegacy();
			}
			Settings::Load();
			Drips::OnDataLoaded();
			Drips::Apply(Settings::Get().bDrips, Settings::Get().bDripsSweat, Settings::Get().bDripsWater);
			Arousal::Init();
			OStim::Init();
			Devious::Init();
			Heat::Refresh();
			Manager::Get().Init();
			if (g_predecessorLoaded) {
				Status::conflict = "Drench: disable WetFunction NG, both paint the same skin";
			} else if (RE::TESDataHandler::GetSingleton()->LookupModByName("WetFunction.esp"sv)) {
				Status::conflict = "Drench: disable Wet Function Redux (WetFunction.esp)";
			}
			if (!Status::conflict.empty()) {
				logger::warn("{}", Status::conflict);
			}
			Status::raceMenu = Skee::Overrides() != nullptr;
			Mdnm::Load();
			Actors::Load();
			Strings::Load();
			logger::info("RaceMenu override interface {} (only used to clean old WetFunction overrides)", Skee::Overrides() ? "found" : "absent");
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			Manager::Get().SetReady(false);
			Visuals::RestoreAll();
			Drips::OnGameChange();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			if (a_message->type == SKSE::MessagingInterface::kNewGame) {
				Drips::OnGameChange();
			}
			Manager::Get().OnGameLoaded();
			Manager::Get().SetReady(true);
			if (!Status::conflict.empty()) {
				RE::SendHUDMessage::ShowHUDMessage(Status::conflict.c_str());
			}
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLogging();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} is loading", plugin->GetName(), plugin->GetVersion().string());

	SKSE::Init(a_skse);
	Drench::Drips::OnLoad();

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::stl::report_and_fail("Unable to register the SKSE message listener.");
	}
	Drench::InstallSerialization();
	SKSE::GetPapyrusInterface()->Register(Drench::Papyrus::Register);

	logger::info("{} loaded", plugin->GetName());
	return true;
}
