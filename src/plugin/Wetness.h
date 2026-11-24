#pragma once

#include "Settings.h"
#include "core/Core.h"

namespace Drench
{
	// The model's state plus what only the running plugin needs.
	struct ActorRecord : State
	{
		float                                 lastGameTime{ -1.0f };  // game days of the previous update
		float                                 headStart{ 0.0f };      // game hours the next update adds (first-start bonus)
		std::int32_t                          ostimClimaxes{ -1 };
		std::chrono::steady_clock::time_point nextUpdate{};
	};

	// Reads the game for one actor and runs the model (core/Core.h). Main thread.
	namespace Wetness
	{
		void  Start(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings);
		void  Update(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings);
		float GameDays();
	}
}
