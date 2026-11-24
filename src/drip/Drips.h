#pragma once

// Drench's door to the embedded DripSim (src/drip): water and sweat drops that run down the player's skin along the live pose.
// Plain types only, so Drench's plugin code never sees DripSim's headers. Call everything on the game's main thread
// unless noted.
namespace Drench::Drips
{
	// SKSE load: a trampoline for the two hooks. Off for good when the separate DripSim.dll is loaded (both would hook and paint).
	void OnLoad();
	// kDataLoaded: settings, hooks, the modes.
	void OnDataLoaded();
	// kPreLoadGame / kNewGame: the player's 3D goes away.
	void OnGameChange();
	// Drench's switches; stops what was switched off.
	void Apply(bool a_drips, bool a_sweat, bool a_water);
	// Any thread. Drench's sweat this moment, 0..1 (0 = none): the sweat drops follow it.
	void Sweat(float a_heat);
	// Any thread. Water poured over the skin at once, 0..1 (a bath, a script).
	void Soak(float a_strength);
	// Any thread. The player was dried or the effect stopped by hand: every drop goes at once (queued to the main thread).
	void Clear();
	// "" while running; else why the drops are off (shown in the menu).
	const char* Status();
}
