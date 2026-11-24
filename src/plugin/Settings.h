#pragma once

#include "core/Config.h"

namespace Drench
{
	// The values of core/Config.h, read from and written to Data/SKSE/Plugins/Drench.ini.
	struct Settings : Config
	{
		// snapshot used by the main thread; Load() replaces it
		static Settings& Get();
		static void      Load();
		static bool      Save();
		static bool      Reset();

		bool Set(std::string_view a_section, std::string_view a_key, const std::string& a_value);
	};
}
