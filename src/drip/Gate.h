#pragma once

// Drench: which of the embedded DripSim modes may start ("rain" = water, "sweat"). Set from Drench's settings by Drips.cpp.
namespace DS::Gate
{
	bool Allowed(const char* a_mode);
}
