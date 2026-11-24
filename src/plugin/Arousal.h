#pragma once

namespace Drench::Arousal
{
	void         Init();
	bool         IsAvailable();
	std::int32_t Get(RE::Actor* a_actor);  // 0-100, -1 when no arousal framework
}
