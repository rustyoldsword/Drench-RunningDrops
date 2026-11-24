#include "Manager.h"
#include "Settings.h"

// Plain C exports for other SKSE plugins, found through GetProcAddress on Drench.dll. Any thread: they only read
// the lock-free player gauge and settings floats.
extern "C"
{
	struct Drench_PlayerGauge
	{
		std::uint32_t size;    // caller sets sizeof(Drench_PlayerGauge); fields past it are not written
		bool          active;  // the player is managed and the game is running
		float         wetness;
		float         rate;    // wetness points per game hour, negative while drying
		float         dry;     // thresholds and cap from the settings
		float         sweat;
		float         soaked;
		float         cap;
	};

	__declspec(dllexport) bool Drench_GetPlayerGauge(Drench_PlayerGauge* a_out)
	{
		if (!a_out || a_out->size < sizeof(Drench_PlayerGauge)) {
			return false;
		}
		const auto  gauge = Drench::Manager::Get().PlayerGauge();
		const auto& settings = Drench::Settings::Get();
		a_out->active = gauge.active && settings.bPlayerEnabled;
		a_out->wetness = gauge.wetness;
		a_out->rate = gauge.rate;
		a_out->dry = settings.fWetnessDry;
		a_out->sweat = settings.fWetnessStart;
		a_out->soaked = settings.fWetnessSoaked;
		a_out->cap = settings.fWetnessCap;
		return true;
	}

	// The same call under the name WetFunction NG exported (same struct layout), so a plugin written for it
	// only has to look in Drench.dll as well.
	__declspec(dllexport) bool WFNG_GetPlayerGauge(Drench_PlayerGauge* a_out)
	{
		return Drench_GetPlayerGauge(a_out);
	}
}
