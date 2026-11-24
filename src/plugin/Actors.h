#pragma once

#include "core/Core.h"

// Per-character modes (normal / shine only / excluded), kept in Data/SKSE/Plugins/Drench_Actors.ini so they hold
// in every save and every new game. Keyed by the character's base record (plugin + local form id), so every
// reference of that character shares the mode; the player is "player".
namespace Drench::Actors
{
	void      Load();  // data loaded; re-reads the file
	ActorMode Mode(RE::Actor* a_actor);
	// Stores the mode (Normal removes the entry) and saves the file. False when the actor has no stable key
	// (a runtime-generated base with no template): such an actor cannot be remembered across saves.
	bool      SetMode(RE::Actor* a_actor, ActorMode a_mode);
	bool      SetMode(const ActorKey& a_key, ActorMode a_mode);

	std::optional<ActorKey> KeyOf(RE::Actor* a_actor);
	std::string             Describe(const ActorKey& a_key);  // "Lydia (Skyrim.esm|0x0A2C94)" when loaded, the key otherwise

	struct Entry
	{
		ActorKey  key;
		ActorMode mode;
	};
	std::vector<Entry> All();

	// Per-character choice of skin wet maps (section [Skins] of the same file): "" = auto, kSharedSkin = shared maps
	// only, otherwise a skin folder key under Drench\skins (core ChosenSkinWetTexture).
	std::string SkinChoice(RE::Actor* a_actor);
	bool        SetSkinChoice(RE::Actor* a_actor, std::string a_choice);  // "" removes the entry
	bool        SetSkinChoice(const ActorKey& a_key, std::string a_choice);

	struct SkinEntry
	{
		ActorKey    key;
		std::string choice;
	};
	std::vector<SkinEntry> AllSkins();
}
