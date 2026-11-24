#pragma once
// Drench - game-independent rules. Nothing in here touches CommonLib, so the whole file is
// unit-tested offline (tests/test_core.cpp). The plugin reads the game, fills Inputs, and acts on the results.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Config.h"

namespace Drench
{
	// ------------------------------------------------------------------ wetness model
	enum class Phase : std::uint8_t
	{
		Dry,
		Sweat,
		Soaked
	};
	std::string_view PhaseName(Phase a_phase);

	// TESWeather classification, plus the two cases that have no weather at all
	enum class Weather : std::uint8_t
	{
		Indoors,
		None,
		Pleasant,
		Cloudy,
		Rain,
		Snow,
		Unclassified
	};
	std::string_view WeatherName(Weather a_weather);
	float            WeatherRate(Weather a_weather, const Config& a_config);

	enum class Movement : std::uint8_t
	{
		Still,
		Sneaking,
		Running,
		Sprinting,
		Galloping  // sprinting on a mount
	};

	// Per-actor state. The first block goes into the co-save, the second is rebuilt on every update.
	struct State
	{
		float  wetness{ 0.0f };
		float  forceWetness{ -1.0f };
		float  forceSpecular{ 0.0f };
		float  forceGlossiness{ 0.0f };
		double autoStop{ 0.0 };  // game days; 0 = not auto-applied
		bool   active{ false };
		bool   manual{ false };  // started by the player/console, never auto-stops
		bool   seeded{ false };  // first start bonus already granted
		Phase  phase{ Phase::Dry };
		bool   hasDrops{ false };
		bool   hasSweat{ false };
		float  dryDyn{ 0.0f };
		float  maxDyn{ 0.0f };
		float  recentStrength{ 0.0f };

		bool         hasPussy{ false };
		bool         hasSchlong{ false };
		float        rate{ 0.0f };
		float        rateActivity{ 0.0f };  // stamina, magicka, movement, work
		float        rateWeather{ 0.0f };
		float        rateHeat{ 0.0f };  // drying next to a fire
		float        rateArousal{ 0.0f };
		float        waterFloor{ 0.0f };  // wetness the water the actor stands in keeps up
		std::int32_t arousal{ -1 };
		Weather      weather{ Weather::None };
		float        sexRate{ 0.0f };
		float        lastStamina{ 1.0f };
		float        lastMagicka{ 1.0f };
	};

	// What the plugin read from the game for one update of one actor.
	struct Inputs
	{
		float        hours{ 0.0f };  // game hours since the previous update
		bool         player{ false };
		float        stamina{ 1.0f };  // 0..1 of the maximum
		float        magicka{ 1.0f };
		Movement     movement{ Movement::Still };
		bool         working{ false };  // at an anvil, grindstone, chopping block...
		Weather      weather{ Weather::None };
		Weather      outgoingWeather{ Weather::None };  // the weather that is fading out
		float        weatherPct{ 1.0f };  // 0..1 how far the current weather has come in
		float        submersion{ 0.0f };  // 0..1 of the actor's height under water; 1 when swimming
		bool         nearHeat{ false };
		std::int32_t arousal{ -1 };  // -1 = no arousal framework
		bool         genitalsBlocked{ false };  // chastity belt or cage
	};

	// Starts (or restarts) tracking. a_random is 0..1 and only used for the first-start bonus.
	// Returns the game hours the caller adds to the first update (the first-start head start, else 0).
	float Start(State& a_state, const Inputs& a_inputs, const Config& a_config, float a_random);
	void Update(State& a_state, const Inputs& a_inputs, const Config& a_config);

	// 0..1: how wet the actor looks within the current phase.
	float Strength(const State& a_state, const Config& a_config);
	// Wetness that standing this deep in water keeps up: 0 below fWaterMinDepth, the cap from fWaterFullDepth.
	float WaterFloor(float a_submersion, const Config& a_config);

	// ------------------------------------------------------------------ drips (the embedded DripSim, player only)
	// How hot the running sweat drops are for this state: 0 = none (not sweating, water on the skin, switched off),
	// else 0.3 at the sweat start up to 1 once soaked with sweat.
	float DripHeat(const State& a_state, const Config& a_config);
	// Water poured over the skin when something sets the wetness from a_before to a_after (a bath, a script):
	// 0 = none (not soaked, not wetter by at least 1, switched off), else 0.4 just soaked up to 1 at the cap.
	float DripSoak(float a_before, float a_after, const Config& a_config);

	// ------------------------------------------------------------------ look
	// What an actor should look like. Floats <= 0 mean "leave the actor's own value".
	struct Look
	{
		bool  active{ false };  // false: everything back to the actor's own values
		bool  drops{ false };
		bool  sweat{ false };
		bool  pussy{ false };
		bool  schlong{ false };
		float specular{ -1.0f };
		float glossiness{ -1.0f };

		bool operator==(const Look&) const = default;
	};
	Look ComputeLook(const State& a_state, const Config& a_config);

	// ------------------------------------------------------------------ textures
	// How a skin takes the wet map.
	enum class SkinMode : std::uint8_t
	{
		None,      // no body geometry
		Specular,  // model space normals: wet map in the specular slot (7), "_s"
		Normal,    // tangent space UBE: wet map replaces the normal map (1), "_UBE_n"
		Floats     // tangent space, not UBE: no matching wet map, specular/glossiness only
	};
	std::string_view SkinModeName(SkinMode a_mode);
	SkinMode         ModeFor(bool a_hasBody, bool a_modelSpaceNormals, std::string_view a_raceEditorID, std::string_view a_bodyNormal);
	constexpr std::uint8_t TextureSlot(SkinMode a_mode) { return a_mode == SkinMode::Normal ? 1 : 7; }

	enum class Part : std::uint8_t
	{
		Body,
		Hands,
		Feet,
		Head,
		Schlong,
		Count
	};
	inline constexpr std::size_t kPartCount = static_cast<std::size_t>(Part::Count);
	std::string_view             PartName(Part a_part);

	// What one part should carry. An empty texture and floats <= 0 mean "the part's own".
	struct Want
	{
		std::string  texture;
		std::string  fallback;  // used when 'texture' is not installed: the one map a part had for every state
		std::uint8_t slot{ 7 };
		float        specular{ -1.0f };
		float        glossiness{ -1.0f };

		bool operator==(const Want&) const = default;
	};
	using Plan = std::array<Want, kPartCount>;

	struct Skin
	{
		SkinMode mode{ SkinMode::None };
		bool     female{ true };
		bool     schlong{ false };  // the actor wears a schlong mesh
		bool     textures{ true };  // false = "shine only": the skin's own maps stay untouched
	};
	Plan PlanLook(const Look& a_look, const Skin& a_skin, const Config& a_config);

	// ------------------------------------------------------------------ actor modes
	// Chosen per character in Drench_Actors.ini. Normal follows the global settings.
	enum class ActorMode : std::uint8_t
	{
		Normal,
		ShineOnly,  // managed, but the skin keeps its own maps: only specular and glossiness change
		Excluded    // never managed: no wetness, no look
	};
	std::string_view          ActorModeName(ActorMode a_mode);  // "normal" | "shine" | "excluded"
	std::optional<ActorMode>  ParseActorMode(std::string_view a_text);

	// [Textures] iTextureMode: which skins get wet maps at all
	enum class TextureMode : std::int32_t
	{
		All = 0,           // every skin with a matching map set
		ShineOnlyUBE = 1,  // UBE skins keep their own normal map (the wet map replaces it, losing skin detail)
		ShineOnly = 2      // no skin gets a map
	};
	// Whether a skin of this mode, on an actor of this mode, receives wet maps.
	bool WantsTextures(SkinMode a_skin, ActorMode a_actor, std::int32_t a_textureMode);

	// "Skyrim.esm|0x01A694" <-> plugin + local form id (no load-order byte); the player is "player"
	struct ActorKey
	{
		std::string   plugin;  // empty for the player
		std::uint32_t local{ 0 };

		bool operator==(const ActorKey&) const = default;
	};
	std::string             FormatActorKey(const ActorKey& a_key);
	std::optional<ActorKey> ParseActorKey(std::string_view a_text);

	// "textures\actors\character\Drench\[male_]<base>_s.dds" or "..._UBE_n.dds"
	std::string WetTexture(bool a_female, SkinMode a_mode, std::string_view a_base);
	// Every map a skin can ask for, to preload.
	std::vector<std::string> WetTextureSet(bool a_female, SkinMode a_mode);

	// The wet map of a Goam Elven Ears - Redone ear (its own specular path a_earSpecular: GoamEar<Sm|Lrg|XL>_s), made
	// for the head map a_head it sits on: "[male_]wethead<_state>(_s|_UBE_n).dds" ->
	// "[male_]wetear_Goam<Size><_state>[_UBE]_s.dds". Goam ears are model-space whatever the body, so the map is a
	// specular one (slot 7). Empty for anything else, or when the head gets no map.
	std::string EarTexture(std::string_view a_head, std::string_view a_earSpecular);

	// A COTR face (Charmers of the Reach and the packs built on it) is told by the face geometry itself, never by a
	// race, mesh or texture name: model-space normals and one of the COTR head topologies (3127 or 12206 vertices,
	// whatever the sex - seen on 1823 FaceGen heads). Its UV layout is its own, so it gets maps made for it.
	bool IsCotrHead(std::uint32_t a_vertexCount, bool a_modelSpaceNormals);
	// The map of a COTR face for the head map a_head planned for the actor's body: "[male_]wethead<_state>(_s|_UBE_n)
	// .dds" -> "wethead_COTR<_state>_s.dds" (women) / "male_wethead_COTR_s.dds" (men: one map). Always a specular map
	// (slot 7): the COTR head is model-space even on a UBE body. 'fallback' is the stateless map. Empty for anything else.
	struct CotrMaps
	{
		std::string texture;
		std::string fallback;
	};
	CotrMaps CotrHeadTextures(std::string_view a_head);

	// The tangent detail map Mu Dynamic NormalMap gets for a wet 3BA (female, specular) body map of the same state:
	// "...\wet_<drops><sweat><pussy>_s.dds" -> "...\wet_<state>_detail_n.dds". Empty for anything else (UBE carries
	// its relief in its own normal map; there are no male detail maps; wet_001 is never planned).
	std::string DetailTexture(std::string_view a_bodyTexture);
	// What a dried actor's detail map goes back to when its skin has none of its own: flat, no effect.
	std::string FlatDetailTexture();

	// A wet map made for one particular skin: the same file name under "...\Drench\skins\<that skin's own normal map
	// path, lower case, without 'textures\' and '.dds'>\". A skin patch (e.g. a cellulite normal) ships its maps there,
	// so only the characters wearing that skin get them; everyone else keeps the shared map. Empty when either path is
	// empty, the wet map is not ours, or the skin path is itself one of our maps.
	std::string SkinWetTexture(std::string_view a_wet, std::string_view a_skinNormal);
	// The folder key of a skin: its normal map path, lower case, back slashes, without 'data\', 'textures\' and '.dds'
	// ("Textures\!UBE\Body\femalebody_1_n.dds" -> "!ube\body\femalebody_1_n"). Empty for an empty path or one of our maps.
	std::string SkinKey(std::string_view a_skinNormal);
	// The wet map of a skin folder key: "...\Drench\skins\<key>\<file name of a_wet>". Empty when either is empty or
	// a_wet is not ours.
	std::string SkinWetTextureForKey(std::string_view a_wet, std::string_view a_key);
	// A character's choice of skin wet maps (Drench_Actors.ini [Skins]): "" = auto (by its own skin), "shared" = only
	// the shared maps, anything else = the maps of that skin folder key, whatever skin the character wears.
	inline constexpr std::string_view kSharedSkin = "shared";
	std::string ChosenSkinWetTexture(std::string_view a_wet, std::string_view a_skinNormal, std::string_view a_choice);

	bool IsOurTexture(std::string_view a_path);     // a Drench wet map
	bool IsLegacyTexture(std::string_view a_path);  // a WetFunction (Redux/NG, Dewpoint) wet map

	// ------------------------------------------------------------------ co-save
	// One blob: u16 version, u16 record size, u32 count, then the records. A newer writer may append fields
	// to a record; this reader takes the part it knows and skips the rest.
	using SavedActor = std::pair<std::uint32_t, State>;  // form id as stored in the save, before resolving

	bool                                   WorthSaving(const State& a_state);
	std::vector<std::uint8_t>              Encode(std::span<const SavedActor> a_actors);
	std::optional<std::vector<SavedActor>> Decode(std::span<const std::uint8_t> a_data);
}
