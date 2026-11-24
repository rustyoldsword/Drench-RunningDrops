#include "core/Core.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <format>

namespace Drench
{
	namespace
	{
		constexpr std::string_view kWetFolder = "textures\\actors\\character\\Drench\\";

		bool IContains(std::string_view a_text, std::string_view a_part)
		{
			const auto same = [](char a_left, char a_right) {
				const auto fold = [](char a_char) { return a_char == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); };
				return fold(a_left) == fold(a_right);
			};
			return std::search(a_text.begin(), a_text.end(), a_part.begin(), a_part.end(), same) != a_text.end();
		}

		void Enter(State& a_state, Phase a_phase, const Config& a_config)
		{
			a_state.phase = a_phase;
			switch (a_phase) {
			case Phase::Dry:
				a_state.hasDrops = false;
				a_state.hasSweat = false;
				break;
			case Phase::Sweat:
				a_state.hasSweat = true;
				a_state.maxDyn = a_config.fWetnessStart;
				a_state.dryDyn = a_state.maxDyn;
				break;
			case Phase::Soaked:
				a_state.hasDrops = true;
				a_state.dryDyn = a_config.fWetnessDry;
				if (!a_state.hasSweat) {
					a_state.recentStrength = Strength(a_state, a_config);
				}
				break;
			}
		}

		void UpdatePhase(State& a_state, const Config& a_config)
		{
			const float wetness = a_state.wetness;
			switch (a_state.phase) {
			case Phase::Dry:
				if (wetness > a_config.fWetnessSoaked) {
					Enter(a_state, Phase::Soaked, a_config);
				} else if (wetness > a_config.fWetnessStart) {
					Enter(a_state, Phase::Sweat, a_config);
				}
				break;
			case Phase::Sweat:
				if (wetness > a_config.fWetnessSoaked) {
					Enter(a_state, Phase::Soaked, a_config);
					break;
				}
				if (wetness > a_state.maxDyn) {
					// the higher the sweat got, the closer to "dry" it has to fall before it vanishes
					const float dry = a_config.fWetnessDry;
					const float start = a_config.fWetnessStart;
					const float span = a_config.fWetnessSoaked - start;
					a_state.maxDyn = wetness;
					a_state.dryDyn = span > 0.0f ? dry - (dry - start) * (1.0f - (wetness - start) / span) : dry;
				}
				if (wetness <= a_state.dryDyn) {
					Enter(a_state, Phase::Dry, a_config);
				}
				break;
			case Phase::Soaked:
				if (wetness <= a_state.dryDyn) {
					Enter(a_state, Phase::Dry, a_config);
					break;
				}
				if (!a_state.hasSweat) {
					// late sweat: soaked by water while dry, then wetness climbs again by the given ratio
					const float strength = Strength(a_state, a_config);
					if (strength < a_state.recentStrength) {
						a_state.recentStrength = strength;
					} else if (strength > a_state.recentStrength + a_config.fLateSweat) {
						a_state.hasSweat = true;
					}
				}
				break;
			}
		}

		float MovementRate(Movement a_movement, const Config& a_config)
		{
			switch (a_movement) {
			case Movement::Sneaking:
				return a_config.fGenerateSneaking;
			case Movement::Running:
				return a_config.fGenerateRunning;
			case Movement::Sprinting:
				return a_config.fGenerateSprinting;
			case Movement::Galloping:
				return a_config.fGenerateGallop;
			default:
				return 0.0f;
			}
		}
	}

	std::string_view PhaseName(Phase a_phase)
	{
		switch (a_phase) {
		case Phase::Sweat:
			return "sweat";
		case Phase::Soaked:
			return "soaked";
		default:
			return "dry";
		}
	}

	std::string_view WeatherName(Weather a_weather)
	{
		switch (a_weather) {
		case Weather::Indoors:
			return "indoors";
		case Weather::Pleasant:
			return "pleasant";
		case Weather::Cloudy:
			return "cloudy";
		case Weather::Rain:
			return "rain";
		case Weather::Snow:
			return "snow";
		case Weather::Unclassified:
			return "unclassified";
		default:
			return "none";
		}
	}

	float WeatherRate(Weather a_weather, const Config& a_config)
	{
		switch (a_weather) {
		case Weather::Pleasant:
			return a_config.fWeatherPleasant;
		case Weather::Cloudy:
			return a_config.fWeatherCloudy;
		case Weather::Rain:
			return a_config.fWeatherRainy;
		case Weather::Snow:
			return a_config.fWeatherSnow;
		default:
			return a_config.fWeatherNone;
		}
	}

	float Strength(const State& a_state, const Config& a_config)
	{
		if (a_state.phase == Phase::Dry) {
			return 0.0f;
		}
		const float span = a_config.fWetnessCap - a_config.fWetnessDry;
		return span > 0.0f ? std::clamp((a_state.wetness - a_state.dryDyn) / span, 0.0f, 1.0f) : 0.0f;
	}

	float WaterFloor(float a_submersion, const Config& a_config)
	{
		if (a_submersion <= 0.0f || a_submersion < a_config.fWaterMinDepth) {
			return 0.0f;
		}
		const float span = a_config.fWaterFullDepth - a_config.fWaterMinDepth;
		const float depth = span > 0.0f ? std::clamp((a_submersion - a_config.fWaterMinDepth) / span, 0.0f, 1.0f) : 1.0f;
		// water leaves drops, so even the least depth that counts lands just inside the soaked phase;
		// from the full depth on the actor is soaked through
		return a_config.fWetnessSoaked + (a_config.fWetnessCap - a_config.fWetnessSoaked) * std::max(depth, 0.05f);
	}

	float DripHeat(const State& a_state, const Config& a_config)
	{
		if (!a_config.bDrips || !a_config.bDripsSweat || !a_state.hasSweat || a_state.wetness <= a_config.fWetnessStart) {
			return 0.0f;
		}
		const float span = a_config.fWetnessSoaked - a_config.fWetnessStart;
		const float level = span > 0.0f ? std::clamp((a_state.wetness - a_config.fWetnessStart) / span, 0.0f, 1.0f) : 1.0f;
		return 0.3f + 0.7f * level;
	}

	float DripSoak(float a_before, float a_after, const Config& a_config)
	{
		if (!a_config.bDrips || !a_config.bDripsWater || a_after < a_config.fWetnessSoaked || a_after - a_before < 1.0f) {
			return 0.0f;
		}
		const float span = a_config.fWetnessCap - a_config.fWetnessSoaked;
		const float level = span > 0.0f ? std::clamp((a_after - a_config.fWetnessSoaked) / span, 0.0f, 1.0f) : 1.0f;
		return 0.4f + 0.6f * level;
	}

	float Start(State& a_state, const Inputs& a_inputs, const Config& a_config, float a_random)
	{
		a_state.active = true;
		a_state.lastStamina = a_inputs.stamina;
		a_state.lastMagicka = a_inputs.magicka;
		// the phase follows from the wetness on the first update
		a_state.phase = Phase::Dry;
		a_state.hasDrops = false;
		a_state.hasSweat = false;
		if (a_state.seeded) {
			return 0.0f;
		}
		// first start: a flat and a random bonus, and the current rate counts as if it already ran for a while
		a_state.seeded = true;
		a_state.wetness = std::clamp(a_config.fAutoBonusNormal + a_config.fAutoBonusRandom * std::clamp(a_random, 0.0f, 1.0f), 0.0f, a_config.fWetnessCap);
		return std::max(a_config.fAutoBonusHours, 0.0f);
	}

	void Update(State& a_state, const Inputs& a_inputs, const Config& a_config)
	{
		const float cap = a_config.fWetnessCap;
		const float hours = std::max(a_inputs.hours, 0.0f);
		const float forced = a_state.forceWetness >= 0.0f ? a_state.forceWetness : a_config.fWetnessForce;

		a_state.arousal = a_inputs.arousal;
		a_state.weather = a_inputs.weather;
		a_state.waterFloor = WaterFloor(a_inputs.submersion, a_config);

		if (forced >= 0.0f) {
			a_state.wetness = std::min(forced, cap);
		} else {
			float used = 0.0f;
			if (a_inputs.stamina < a_state.lastStamina) {
				used += (a_state.lastStamina - a_inputs.stamina) * a_config.fGenerateStamina;
			}
			if (a_inputs.magicka < a_state.lastMagicka) {
				used += (a_state.lastMagicka - a_inputs.magicka) * a_config.fGenerateMagicka;
			}
			float activity = hours > 0.0f ? used / hours : 0.0f;
			activity += MovementRate(a_inputs.movement, a_config);
			if (a_inputs.working) {
				activity += a_config.fGenerateWorking;
			}

			const float t = std::clamp(a_inputs.weatherPct, 0.0f, 1.0f);
			const float weather = t * WeatherRate(a_inputs.weather, a_config) + (1.0f - t) * WeatherRate(a_inputs.outgoingWeather, a_config);

			// arousal makes skin sweaty but does not keep an actor soaked
			const float aroused = a_inputs.arousal >= 0 && a_state.wetness < a_config.fWetnessSoaked ? static_cast<float>(a_inputs.arousal) * a_config.fGenerateArousal * 0.01f : 0.0f;
			const float heat = a_config.bHeatDrying && a_inputs.nearHeat && a_state.wetness > 0.0f ? -a_config.fHeatDrying : 0.0f;

			float rate = activity + weather + heat + aroused + a_state.sexRate;
			if (!a_inputs.player) {
				rate += a_config.fOtherAdd;
				rate *= a_config.fOtherMult;
			}
			rate *= a_config.fMultGlobal;

			a_state.rateActivity = activity;
			a_state.rateWeather = weather;
			a_state.rateHeat = heat;
			a_state.rateArousal = aroused;
			a_state.rate = rate;
			a_state.wetness = std::clamp(a_state.wetness + rate * hours, 0.0f, cap);

			if (a_state.waterFloor > 0.0f) {
				a_state.wetness = std::clamp(a_state.wetness, std::min(a_state.waterFloor, cap), cap);
				// In water no rise counts as late sweat: the floor steps up as the actor wades deeper, and running
				// through the water lifts the wetness a little above it (that read as sweat in game, 22.11).
				// The phase update lowers this to the current strength, so sweat starts counting from the shore.
				a_state.recentStrength = 1.0f;
			}
		}
		a_state.lastStamina = a_inputs.stamina;
		a_state.lastMagicka = a_inputs.magicka;

		// a wet schlong from a quarter of the cap; the pussy texture from arousal when configured
		a_state.hasSchlong = a_state.wetness > cap * 0.25f;
		a_state.hasPussy = a_state.hasSchlong;
		if (a_inputs.arousal >= 0 && a_config.bUseArousalThreshold) {
			a_state.hasPussy = static_cast<float>(a_inputs.arousal) > a_config.fArousedThreshold;
		}
		// a chastity belt/cage physically covers what the pussy/schlong texture would show
		if (a_config.bDeviousBlockGenitals && a_inputs.genitalsBlocked) {
			a_state.hasPussy = false;
			a_state.hasSchlong = false;
		}

		UpdatePhase(a_state, a_config);
	}

	Look ComputeLook(const State& a_state, const Config& a_config)
	{
		Look look;
		look.active = a_state.active;
		look.drops = a_state.hasDrops;
		look.sweat = a_state.hasSweat;
		look.pussy = a_state.hasPussy;
		look.schlong = a_state.hasSchlong;

		const bool  wet = a_state.phase != Phase::Dry;
		const float strength = Strength(a_state, a_config);
		const float forcedSpecular = a_state.forceSpecular > 0.0f ? a_state.forceSpecular : a_config.fSpecularForce;
		const float forcedGlossiness = a_state.forceGlossiness > 0.0f ? a_state.forceGlossiness : a_config.fGlossinessForce;
		// dry actors keep their own shader values
		look.specular = forcedSpecular > 0.0f ? forcedSpecular : (wet ? a_config.fSpecularMin + (a_config.fSpecularMax - a_config.fSpecularMin) * strength : -1.0f);
		look.glossiness = forcedGlossiness > 0.0f ? forcedGlossiness : (wet ? a_config.fGlossinessMin + (a_config.fGlossinessMax - a_config.fGlossinessMin) * strength : -1.0f);
		return look;
	}

	// ---------------------------------------------------------------------- textures
	std::string_view SkinModeName(SkinMode a_mode)
	{
		switch (a_mode) {
		case SkinMode::Specular:
			return "specular slot 7 (_s)";
		case SkinMode::Normal:
			return "normal slot 1 (_UBE_n)";
		case SkinMode::Floats:
			return "floats only";
		default:
			return "no skin";
		}
	}

	SkinMode ModeFor(bool a_hasBody, bool a_modelSpaceNormals, std::string_view a_raceEditorID, std::string_view a_bodyNormal)
	{
		if (!a_hasBody) {
			return SkinMode::None;
		}
		if (a_modelSpaceNormals) {
			return SkinMode::Specular;
		}
		if (IContains(a_raceEditorID, "UBE") || IContains(a_bodyNormal, "UBE")) {
			return SkinMode::Normal;
		}
		return SkinMode::Floats;
	}

	std::string_view PartName(Part a_part)
	{
		switch (a_part) {
		case Part::Body:
			return "body";
		case Part::Hands:
			return "hands";
		case Part::Feet:
			return "feet";
		case Part::Head:
			return "head";
		case Part::Schlong:
			return "schlong";
		default:
			return "?";
		}
	}

	std::string WetTexture(bool a_female, SkinMode a_mode, std::string_view a_base)
	{
		return std::format("{}{}{}{}", kWetFolder, a_female ? "" : "male_", a_base, a_mode == SkinMode::Normal ? "_UBE_n.dds" : "_s.dds");
	}

	std::vector<std::string> WetTextureSet(bool a_female, SkinMode a_mode)
	{
		std::vector<std::string> result;
		if (a_mode != SkinMode::Specular && a_mode != SkinMode::Normal) {
			return result;
		}
		const auto bases = a_female ? std::vector<std::string_view>{ "wet_001", "wet_010", "wet_011", "wet_100", "wet_101", "wet_110", "wet_111", "wethand", "wethead" } :
		                              std::vector<std::string_view>{ "wet_01", "wet_10", "wet_11", "wethand", "wethead" };
		for (const auto base : bases) {
			result.push_back(WetTexture(a_female, a_mode, base));
		}
		// a futanari schlong is an SOS/TNG mesh, not UBE skin: it keeps the specular map
		result.push_back(WetTexture(a_female, a_female ? SkinMode::Specular : a_mode, "wetschlong"));
		return result;
	}

	std::string EarTexture(std::string_view a_head, std::string_view a_earSpecular)
	{
		using namespace std::literals;
		const auto fileName = [](std::string_view a_path) {
			const auto slash = a_path.find_last_of("\\/");
			std::string name(slash == std::string_view::npos ? a_path : a_path.substr(slash + 1));
			std::ranges::transform(name, name.begin(), [](char a_char) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); });
			return name;
		};
		const auto ear = fileName(a_earSpecular);
		std::string_view size;
		for (const auto [file, title] : { std::pair{ "goamearsm_s.dds"sv, "Sm"sv }, std::pair{ "goamearlrg_s.dds"sv, "Lrg"sv }, std::pair{ "goamearxl_s.dds"sv, "XL"sv } }) {
			if (ear == file) {
				size = title;
			}
		}
		const auto head = fileName(a_head);
		const bool male = head.starts_with("male_wethead");
		if (size.empty() || !(male || head.starts_with("wethead"))) {
			return {};
		}
		std::string_view rest(head);
		rest.remove_prefix(male ? "male_wethead"sv.size() : "wethead"sv.size());
		const bool ube = rest.ends_with("_ube_n.dds");
		if (!ube && !rest.ends_with("_s.dds")) {
			return {};
		}
		rest.remove_suffix(ube ? "_ube_n.dds"sv.size() : "_s.dds"sv.size());  // what is left is the state: "", "_10" ...
		return std::format("{}{}wetear_Goam{}{}{}_s.dds", kWetFolder, male ? "male_" : "", size, rest, ube ? "_UBE" : "");
	}

	bool IsCotrHead(std::uint32_t a_vertexCount, bool a_modelSpaceNormals)
	{
		return a_modelSpaceNormals && (a_vertexCount == 3127 || a_vertexCount == 12206);
	}

	CotrMaps CotrHeadTextures(std::string_view a_head)
	{
		using namespace std::literals;
		const auto  slash = a_head.find_last_of("\\/");
		std::string head(slash == std::string_view::npos ? a_head : a_head.substr(slash + 1));
		std::ranges::transform(head, head.begin(), [](char a_char) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); });
		const bool male = head.starts_with("male_wethead");
		if (!male && !head.starts_with("wethead")) {
			return {};
		}
		std::string_view rest(head);
		rest.remove_prefix(male ? "male_wethead"sv.size() : "wethead"sv.size());
		const bool ube = rest.ends_with("_ube_n.dds");
		if (!ube && !rest.ends_with("_s.dds")) {
			return {};
		}
		rest.remove_suffix(ube ? "_ube_n.dds"sv.size() : "_s.dds"sv.size());  // the state: "", "_10" ...
		if (male) {
			return { std::format("{}male_wethead_COTR_s.dds", kWetFolder), {} };
		}
		auto single = std::format("{}wethead_COTR_s.dds", kWetFolder);
		if (rest.empty()) {
			return { std::move(single), {} };
		}
		return { std::format("{}wethead_COTR{}_s.dds", kWetFolder, rest), std::move(single) };
	}

	std::string DetailTexture(std::string_view a_bodyTexture)
	{
		using namespace std::literals;
		const auto slash = a_bodyTexture.find_last_of("\\/");
		std::string name(slash == std::string_view::npos ? a_bodyTexture : a_bodyTexture.substr(slash + 1));
		std::ranges::transform(name, name.begin(), [](char a_char) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); });
		for (const auto state : { "010"sv, "011"sv, "100"sv, "101"sv, "110"sv, "111"sv }) {
			if (name == std::format("wet_{}_s.dds", state)) {
				return std::format("{}wet_{}_detail_n.dds", kWetFolder, state);
			}
		}
		return {};
	}

	std::string FlatDetailTexture() { return std::format("{}wet_flat_detail_n.dds", kWetFolder); }

	bool IsOurTexture(std::string_view a_path) { return IContains(a_path, "\\Drench\\"); }

	namespace
	{
		std::string LowerBackslash(std::string_view a_text)
		{
			std::string out(a_text);
			std::ranges::transform(out, out.begin(), [](char a_char) {
				return a_char == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(a_char)));
			});
			return out;
		}
	}

	std::string SkinKey(std::string_view a_skinNormal)
	{
		using namespace std::literals;
		if (a_skinNormal.empty() || IsOurTexture(a_skinNormal)) {
			return {};
		}
		const auto       key = LowerBackslash(a_skinNormal);
		std::string_view rest(key);
		for (const auto prefix : { "data\\"sv, "textures\\"sv }) {
			if (rest.starts_with(prefix)) {
				rest.remove_prefix(prefix.size());
			}
		}
		if (rest.ends_with(".dds")) {
			rest.remove_suffix(".dds"sv.size());
		}
		return std::string(rest);
	}

	std::string SkinWetTextureForKey(std::string_view a_wet, std::string_view a_key)
	{
		if (a_wet.empty() || a_key.empty() || !IsOurTexture(a_wet)) {
			return {};
		}
		const auto slash = a_wet.find_last_of("\\/");
		return std::format("{}skins\\{}\\{}", kWetFolder, LowerBackslash(a_key), a_wet.substr(slash + 1));
	}

	std::string SkinWetTexture(std::string_view a_wet, std::string_view a_skinNormal)
	{
		return SkinWetTextureForKey(a_wet, SkinKey(a_skinNormal));
	}

	std::string ChosenSkinWetTexture(std::string_view a_wet, std::string_view a_skinNormal, std::string_view a_choice)
	{
		if (a_choice.empty()) {
			return SkinWetTexture(a_wet, a_skinNormal);
		}
		if (IContains(a_choice, kSharedSkin) && a_choice.size() == kSharedSkin.size()) {
			return {};
		}
		return SkinWetTextureForKey(a_wet, a_choice);
	}
	bool IsLegacyTexture(std::string_view a_path) { return IContains(a_path, "\\WetFunction\\"); }

	Plan PlanLook(const Look& a_look, const Skin& a_skin, const Config& a_config)
	{
		Plan plan{};
		for (auto& want : plan) {
			want.slot = TextureSlot(a_skin.mode);
		}
		if (!a_look.active || a_skin.mode == SkinMode::None) {
			return plan;
		}
		const auto at = [&](Part a_part) -> Want& { return plan[static_cast<std::size_t>(a_part)]; };

		if (a_skin.textures && a_skin.mode != SkinMode::Floats) {
			const auto bodyTexture = [&](bool a_drops, bool a_sweat, bool a_pussy) -> std::string {
				// the pussy digit is only a variant of the sweat/drop maps; on its own it put a wet body on
				// dry actors while head, hands and feet stayed dry
				if (!a_config.bTextureBody || !(a_drops || a_sweat)) {
					return {};
				}
				const auto base = a_skin.female ? std::format("wet_{}{}{}", int(a_drops), int(a_sweat), int(a_pussy)) : std::format("wet_{}{}", int(a_drops), int(a_sweat));
				return WetTexture(a_skin.female, a_skin.mode, base);
			};
			const bool drops = a_look.drops && a_config.bTextureBodyDrops;
			const bool sweat = a_look.sweat && a_config.bTextureBodySweat;
			const bool pussy = a_look.pussy && a_config.bTextureBodyPussy;
			at(Part::Body).texture = bodyTexture(drops, sweat, pussy);

			const bool feetDrops = a_look.drops && a_config.bTextureFeetDrops;
			const bool feetSweat = a_look.sweat && a_config.bTextureFeetSweat;
			at(Part::Feet).texture = bodyTexture(feetDrops, feetSweat, pussy && (feetDrops || feetSweat));

			if (a_look.drops || a_look.sweat) {
				// Head and hands wear the map of the body's state ("wethead_10": drops, "_01": sweat, "_11": both),
				// matched to the body map at the neck and the wrists. One map for every state left a step in shine
				// there in every state but one.
				const bool body = drops || sweat;  // false: the body's map is switched off, go by the look itself
				const auto state = std::format("_{}{}", int(body ? drops : a_look.drops), int(body ? sweat : a_look.sweat));
				const auto part = [&](Part a_part, std::string_view a_base) {
					at(a_part).texture = WetTexture(a_skin.female, a_skin.mode, std::string(a_base) + state);
					at(a_part).fallback = WetTexture(a_skin.female, a_skin.mode, a_base);
				};
				if (a_config.bTextureHands) {
					part(Part::Hands, "wethand");
				}
				if (a_config.bTextureHead) {
					part(Part::Head, "wethead");
				}
			}
			if (a_look.schlong && a_skin.schlong && (a_skin.female ? a_config.bTextureFutaSchlong : a_config.bTextureSchlong)) {
				const auto mode = a_skin.female ? SkinMode::Specular : a_skin.mode;
				at(Part::Schlong).texture = WetTexture(a_skin.female, mode, "wetschlong");
				at(Part::Schlong).slot = TextureSlot(mode);
			}
		}

		const auto floats = [&](Part a_part, bool a_specular, bool a_glossiness, float a_mult) {
			if (a_specular && a_look.specular > 0.0f) {
				at(a_part).specular = a_look.specular * a_mult;
			}
			if (a_glossiness && a_look.glossiness > 0.0f) {
				at(a_part).glossiness = a_look.glossiness;
			}
		};
		floats(Part::Body, a_config.bSpecularBody, a_config.bGlossinessBody, 1.0f);
		floats(Part::Schlong, a_config.bSpecularBody, a_config.bGlossinessBody, 1.0f);
		floats(Part::Hands, a_config.bSpecularHands, a_config.bGlossinessHands, a_config.fHandSpecularMult);
		floats(Part::Feet, a_config.bSpecularFeet, a_config.bGlossinessFeet, 1.0f);
		floats(Part::Head, a_config.bSpecularHead, a_config.bGlossinessHead, a_config.fHeadSpecularMult);
		return plan;
	}

	// ---------------------------------------------------------------------- actor modes
	std::string_view ActorModeName(ActorMode a_mode)
	{
		switch (a_mode) {
		case ActorMode::ShineOnly:
			return "shine";
		case ActorMode::Excluded:
			return "excluded";
		default:
			return "normal";
		}
	}

	std::optional<ActorMode> ParseActorMode(std::string_view a_text)
	{
		const auto is = [&](std::string_view a_word) {
			return a_text.size() == a_word.size() && IContains(a_text, a_word);
		};
		if (is("normal")) {
			return ActorMode::Normal;
		}
		if (is("shine") || is("shineonly") || is("shine only")) {
			return ActorMode::ShineOnly;
		}
		if (is("excluded") || is("exclude") || is("off")) {
			return ActorMode::Excluded;
		}
		return std::nullopt;
	}

	bool WantsTextures(SkinMode a_skin, ActorMode a_actor, std::int32_t a_textureMode)
	{
		if (a_actor != ActorMode::Normal || (a_skin != SkinMode::Specular && a_skin != SkinMode::Normal)) {
			return false;
		}
		switch (static_cast<TextureMode>(a_textureMode)) {
		case TextureMode::ShineOnly:
			return false;
		case TextureMode::ShineOnlyUBE:
			return a_skin != SkinMode::Normal;
		default:
			return true;
		}
	}

	std::string FormatActorKey(const ActorKey& a_key)
	{
		return a_key.plugin.empty() ? std::string("player") : std::format("{}|0x{:06X}", a_key.plugin, a_key.local);
	}

	std::optional<ActorKey> ParseActorKey(std::string_view a_text)
	{
		const auto trim = [](std::string_view a_part) {
			while (!a_part.empty() && std::isspace(static_cast<unsigned char>(a_part.front()))) {
				a_part.remove_prefix(1);
			}
			while (!a_part.empty() && std::isspace(static_cast<unsigned char>(a_part.back()))) {
				a_part.remove_suffix(1);
			}
			return a_part;
		};
		a_text = trim(a_text);
		if (a_text.size() == 6 && IContains(a_text, "player")) {
			return ActorKey{};
		}
		const auto bar = a_text.rfind('|');
		if (bar == std::string_view::npos) {
			return std::nullopt;
		}
		const auto plugin = trim(a_text.substr(0, bar));
		auto       number = trim(a_text.substr(bar + 1));
		if (number.size() > 2 && number[0] == '0' && (number[1] == 'x' || number[1] == 'X')) {
			number.remove_prefix(2);
		}
		if (plugin.empty() || number.empty() || number.size() > 8) {
			return std::nullopt;
		}
		std::uint32_t value = 0;
		for (const char c : number) {
			const int digit = std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : (std::isxdigit(static_cast<unsigned char>(c)) ? std::tolower(c) - 'a' + 10 : -1);
			if (digit < 0) {
				return std::nullopt;
			}
			value = value * 16 + static_cast<std::uint32_t>(digit);
		}
		// a full form id may be pasted: keep the local part (light plugins use 12 bits, others 24)
		return ActorKey{ std::string(plugin), value & 0x00FFFFFF };
	}

	// ---------------------------------------------------------------------- co-save
	namespace
	{
		constexpr std::uint16_t kSaveVersion = 1;
		// id, 4 floats, autoStop, flag bits, phase, 3 floats
		constexpr std::size_t kRecordSize = 4 + 4 * 4 + 8 + 1 + 1 + 3 * 4;
		constexpr std::size_t kHeaderSize = 2 + 2 + 4;

		enum : std::uint8_t
		{
			kActive = 1 << 0,
			kManual = 1 << 1,
			kSeeded = 1 << 2,
			kDrops = 1 << 3,
			kSweat = 1 << 4
		};

		template <class T>
		void Put(std::vector<std::uint8_t>& a_out, T a_value)
		{
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(std::addressof(a_value));
			a_out.insert(a_out.end(), bytes, bytes + sizeof(T));
		}

		template <class T>
		T Take(const std::uint8_t*& a_cursor)
		{
			T value;
			std::memcpy(std::addressof(value), a_cursor, sizeof(T));
			a_cursor += sizeof(T);
			return value;
		}

		// a damaged save must not put NaN into the model
		float Finite(float a_value, float a_fallback) { return std::isfinite(a_value) ? a_value : a_fallback; }
	}

	bool WorthSaving(const State& a_state)
	{
		return a_state.active || a_state.wetness > 0.0f || a_state.forceWetness >= 0.0f || a_state.forceSpecular > 0.0f || a_state.forceGlossiness > 0.0f;
	}

	std::vector<std::uint8_t> Encode(std::span<const SavedActor> a_actors)
	{
		std::vector<std::uint8_t> out;
		out.reserve(kHeaderSize + a_actors.size() * kRecordSize);
		Put(out, kSaveVersion);
		Put(out, static_cast<std::uint16_t>(kRecordSize));
		Put(out, static_cast<std::uint32_t>(a_actors.size()));
		for (const auto& [id, state] : a_actors) {
			Put(out, id);
			Put(out, state.wetness);
			Put(out, state.forceWetness);
			Put(out, state.forceSpecular);
			Put(out, state.forceGlossiness);
			Put(out, state.autoStop);
			Put(out, static_cast<std::uint8_t>((state.active ? kActive : 0) | (state.manual ? kManual : 0) | (state.seeded ? kSeeded : 0) | (state.hasDrops ? kDrops : 0) |
											   (state.hasSweat ? kSweat : 0)));
			Put(out, static_cast<std::uint8_t>(state.phase));
			Put(out, state.dryDyn);
			Put(out, state.maxDyn);
			Put(out, state.recentStrength);
		}
		return out;
	}

	std::optional<std::vector<SavedActor>> Decode(std::span<const std::uint8_t> a_data)
	{
		if (a_data.size() < kHeaderSize) {
			return std::nullopt;
		}
		const auto* cursor = a_data.data();
		const auto  version = Take<std::uint16_t>(cursor);
		const auto  recordSize = Take<std::uint16_t>(cursor);
		const auto  count = Take<std::uint32_t>(cursor);
		if (version == 0 || recordSize < kRecordSize || (a_data.size() - kHeaderSize) / recordSize < count) {
			return std::nullopt;
		}
		std::vector<SavedActor> actors;
		actors.reserve(count);
		for (std::uint32_t i = 0; i < count; ++i) {
			const auto* next = cursor + recordSize;
			State       state;
			const auto  id = Take<std::uint32_t>(cursor);
			state.wetness = std::max(Finite(Take<float>(cursor), 0.0f), 0.0f);
			state.forceWetness = Finite(Take<float>(cursor), -1.0f);
			state.forceSpecular = Finite(Take<float>(cursor), 0.0f);
			state.forceGlossiness = Finite(Take<float>(cursor), 0.0f);
			const auto autoStop = Take<double>(cursor);
			state.autoStop = std::isfinite(autoStop) ? autoStop : 0.0;
			const auto flags = Take<std::uint8_t>(cursor);
			state.active = flags & kActive;
			state.manual = flags & kManual;
			state.seeded = flags & kSeeded;
			state.hasDrops = flags & kDrops;
			state.hasSweat = flags & kSweat;
			state.phase = static_cast<Phase>(std::min<std::uint8_t>(Take<std::uint8_t>(cursor), static_cast<std::uint8_t>(Phase::Soaked)));
			state.dryDyn = Finite(Take<float>(cursor), 0.0f);
			state.maxDyn = Finite(Take<float>(cursor), 0.0f);
			state.recentStrength = Finite(Take<float>(cursor), 0.0f);
			cursor = next;
			if (id != 0) {
				actors.emplace_back(id, state);
			}
		}
		return actors;
	}
}
