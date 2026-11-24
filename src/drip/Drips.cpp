// SPDX-License-Identifier: GPL-3.0-or-later
// Drench: the embedded DripSim framework and its water and sweat modes, driven by Drench's own wetness.
// Replaces DripSim's main.cpp, RainMain.cpp and SweatMain.cpp: one plugin, no mode DLLs, no mod events.
#include "Drips.h"

#include "Core.h"
#include "Gate.h"
#include "Params.h"
#include "RainMode.h"
#include "Spike.h"
#include "SweatMode.h"

namespace
{
	std::atomic<bool> g_off{ false };  // the separate DripSim.dll is loaded
	std::atomic<bool> g_ready{ false };
	std::atomic<bool> g_water{ true };
	std::atomic<bool> g_sweat{ true };
}

namespace DS::Gate
{
	bool Allowed(const char* a_mode)
	{
		if (g_off || !a_mode) {
			return false;
		}
		const std::string_view mode = a_mode;
		if (mode == "rain") {
			return g_water;
		}
		if (mode == "sweat") {
			return g_sweat;
		}
		return false;
	}
}

namespace Drench::Drips
{
	void OnLoad()
	{
		if (::GetModuleHandleW(L"DripSim.dll")) {
			g_off = true;
			logger::warn("Drips: the separate DripSim.dll is loaded; Drench's drops stay off (disable the DripSim mod, Drench carries it now)");
			return;
		}
		SKSE::AllocTrampoline(64);
	}

	void OnDataLoaded()
	{
		if (g_off) {
			return;
		}
		DS::Params::Load();
		DS::Spike::Install();
		if (!DS::Spike::Installed()) {
			return;  // logged; Status() says why
		}
		auto* api = DS::Core::Api();
		RainLoadSettings();
		SweatLoadSettings();
		const bool rain = api->RegisterMode(RainModeInstance());
		const bool sweat = api->RegisterMode(SweatModeInstance());
		g_ready = rain && sweat;
		logger::info("Drips: DripSim framework {} (water mode {}, sweat mode {})", g_ready ? "ready" : "FAILED", rain ? "on" : "failed", sweat ? "on" : "failed");
	}

	void OnGameChange()
	{
		if (g_ready) {
			DS::Core::StopAll(DripSim::StopReason::BodyLost);
		}
	}

	void Clear()
	{
		if (!g_ready) {
			return;
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				DS::Core::StopAll(DripSim::StopReason::Requested);
				logger::info("Drips: cleared (the player was dried or the effect stopped)");
			});
		}
	}

	void Apply(bool a_drips, bool a_sweat, bool a_water)
	{
		const bool sweat = a_drips && a_sweat;
		const bool water = a_drips && a_water;
		g_sweat = sweat;
		g_water = water;
		if (!g_ready) {
			return;
		}
		auto* api = DS::Core::Api();
		if (!sweat && api->IsActive("sweat")) {
			api->Stop("sweat");
		}
		if (!water && api->IsActive("rain")) {
			api->Stop("rain");
		}
	}

	void Sweat(float a_heat)
	{
		if (g_ready && g_sweat && a_heat > 0.0f) {
			SweatRequestHeat(a_heat);  // picked up by the sweat mode's next Idle or Update
		}
	}

	void Soak(float a_strength)
	{
		if (g_ready && g_water && a_strength > 0.0f) {
			RainRequestSoak(a_strength, 1000.0f);  // the whole body; the water mode starts on its next Idle
			logger::info("Drips: soak {:.2f}", a_strength);
		}
	}

	const char* Status()
	{
		if (g_off) {
			return "Off: the separate DripSim mod is enabled. Disable it, Drench carries it now.";
		}
		if (!g_ready) {
			return "Off: the drop framework did not start (see Drench.log).";
		}
		return "";
	}
}
