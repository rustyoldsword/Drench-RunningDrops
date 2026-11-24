#pragma once

#include "core/Core.h"

namespace Drench
{
	struct Settings;
}

// The wet look, written straight into the skin's shader materials. Nothing is stored in RaceMenu or in the save:
// the values an actor had before are remembered here and put back when the look goes away. Main thread only.
namespace Drench::Visuals
{
	// Brings every skin part of the actor to the look. Cheap to call on every update: a part that already
	// carries it is left alone, a part that was rebuilt (outfit change, 3D reload) is found dry and painted again.
	void Apply(RE::Actor* a_actor, const Look& a_look, const Settings& a_settings);
	// Puts the actor's own values back and forgets the actor.
	void Clear(RE::Actor* a_actor);
	// Puts every tracked actor's own values back. Before a save is loaded: the engine may keep a body across the load.
	void RestoreAll();
	void Forget(RE::FormID a_actor);

	// Removes the RaceMenu overrides WetFunction (Redux, NG 1.x, Dewpoint 1.0) left on an actor. a_everything also
	// drops every specular/glossiness skin override, whoever set it; without it those go only when a WetFunction
	// texture override proves the actor was managed. Returns the number removed.
	std::size_t CleanLegacy(RE::Actor* a_actor, bool a_everything);

	bool HasSchlong(RE::Actor* a_actor);
	// the UV layout of the schlong the actor wears; the wet schlong maps exist for SOS and for LDD's TNG meshes
	enum class Layout : std::uint8_t
	{
		Other,
		Sos,
		LddTng
	};
	Layout SchlongLayout(RE::Actor* a_actor);

	// for the diagnostics page
	struct Summary
	{
		std::size_t              actors{ 0 };      // actors with a painted part
		std::size_t              geometries{ 0 };  // painted parts
		std::size_t              textures{ 0 };    // wet maps held loaded
		std::vector<std::string> sets;             // "female specular: 13 of 13 found"
	};
	Summary Summarize();
	void Dump(RE::Actor* a_actor, std::string_view a_title);
	void Out(const std::string& a_line);
}
