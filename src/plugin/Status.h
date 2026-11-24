#pragma once

// What the diagnostics page shows that is found once at startup. Written on the main thread during startup,
// read by the menu afterwards.
namespace Drench::Status
{
	inline std::string conflict;        // a mod that paints the same skin, or "" when there is none
	inline std::string settingsSource;  // where the settings came from
	inline bool        raceMenu{ false };
}
