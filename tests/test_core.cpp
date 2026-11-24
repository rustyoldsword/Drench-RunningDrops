#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cmath>
#include <limits>

#include "core/Core.h"

using namespace Drench;

namespace
{
	// an actor standing indoors in weather that neither wets nor dries, so a test sees only what it sets
	Config Still()
	{
		Config config;
		config.fWeatherNone = 0.0f;
		return config;
	}

	State Started(const Config& a_config)
	{
		State state;
		Start(state, Inputs{}, a_config, 0.0f);
		return state;
	}

	Inputs Hour(float a_hours = 1.0f)
	{
		Inputs inputs;
		inputs.hours = a_hours;
		inputs.player = true;
		inputs.weather = Weather::Indoors;
		inputs.outgoingWeather = Weather::Indoors;
		return inputs;
	}

	const Want& At(const Plan& a_plan, Part a_part) { return a_plan[static_cast<std::size_t>(a_part)]; }

	constexpr auto kFolder = "textures\\actors\\character\\Drench\\";
	std::string    Tex(std::string_view a_name) { return std::string(kFolder) + std::string(a_name); }
}

// ---------------------------------------------------------------------- wetness model
TEST_CASE("running makes an actor sweat, then soaked, and the look follows")
{
	const auto config = Still();
	auto       state = Started(config);
	auto       inputs = Hour();
	inputs.movement = Movement::Sprinting;  // +1 per hour

	Update(state, inputs, config);
	Update(state, inputs, config);
	CHECK(state.wetness == doctest::Approx(2.0f));
	CHECK(state.phase == Phase::Dry);  // "start" has to be passed, not touched
	CHECK(ComputeLook(state, config).specular < 0.0f);

	Update(state, inputs, config);
	CHECK(state.phase == Phase::Sweat);
	CHECK(state.hasSweat);
	CHECK_FALSE(state.hasDrops);

	Update(state, inputs, config);
	Update(state, inputs, config);
	CHECK(state.wetness == doctest::Approx(5.0f));
	CHECK(state.phase == Phase::Soaked);
	CHECK(state.hasDrops);
	CHECK(state.hasSweat);

	const auto look = ComputeLook(state, config);
	CHECK(look.active);
	CHECK(look.drops);
	// soaked dries at 1, the cap is 10: strength (5 - 1) / 9
	CHECK(Strength(state, config) == doctest::Approx(4.0f / 9.0f));
	CHECK(look.specular == doctest::Approx(3.0f + 9.0f * 4.0f / 9.0f));
	CHECK(look.glossiness == doctest::Approx(40.0f - 15.0f * 4.0f / 9.0f));
}

TEST_CASE("the higher the sweat got, the lower it has to fall before it is gone")
{
	const auto config = Still();
	auto       state = Started(config);
	state.wetness = 3.0f;
	Update(state, Hour(0.0f), config);  // enters sweat
	Update(state, Hour(0.0f), config);  // records the peak 3.0: gone at 1.5
	REQUIRE(state.phase == Phase::Sweat);
	CHECK(state.dryDyn == doctest::Approx(1.5f));

	state.wetness = 1.8f;  // under "start", but the sweat is still there
	Update(state, Hour(0.0f), config);
	CHECK(state.phase == Phase::Sweat);

	state.wetness = 1.5f;
	Update(state, Hour(0.0f), config);
	CHECK(state.phase == Phase::Dry);
	CHECK_FALSE(state.hasSweat);
	CHECK_FALSE(ComputeLook(state, config).sweat);
}

TEST_CASE("wetness never leaves 0..cap and dries by the weather")
{
	Config config;  // indoors dries at 2 per hour
	auto   state = Started(config);
	state.wetness = 3.0f;
	Update(state, Hour(1.0f), config);
	CHECK(state.wetness == doctest::Approx(1.0f));
	Update(state, Hour(5.0f), config);
	CHECK(state.wetness == 0.0f);

	auto inputs = Hour(100.0f);
	inputs.weather = Weather::Rain;
	inputs.outgoingWeather = Weather::Rain;
	Update(state, inputs, config);
	CHECK(state.wetness == config.fWetnessCap);
}

TEST_CASE("weather rates and the blend while one weather fades into another")
{
	Config config;
	CHECK(WeatherRate(Weather::Rain, config) == 3.0f);
	CHECK(WeatherRate(Weather::Snow, config) == 0.5f);
	CHECK(WeatherRate(Weather::Pleasant, config) == -2.0f);
	CHECK(WeatherRate(Weather::Cloudy, config) == -0.2f);
	CHECK(WeatherRate(Weather::Indoors, config) == config.fWeatherNone);
	CHECK(WeatherRate(Weather::Unclassified, config) == config.fWeatherNone);

	auto state = Started(config);
	auto inputs = Hour(0.0f);
	inputs.weather = Weather::Rain;
	inputs.outgoingWeather = Weather::Pleasant;
	inputs.weatherPct = 0.25f;
	Update(state, inputs, config);
	CHECK(state.rateWeather == doctest::Approx(0.25f * 3.0f + 0.75f * -2.0f));
	CHECK(state.weather == Weather::Rain);
}

TEST_CASE("spent stamina and magicka count as activity; NPCs get their own add and multiplier")
{
	auto config = Still();
	config.fOtherAdd = 1.0f;
	config.fOtherMult = 2.0f;
	auto state = Started(config);
	auto inputs = Hour(0.5f);
	inputs.stamina = 0.6f;  // 0.4 spent * 1.5 = 0.6 over half an hour = 1.2 per hour
	inputs.magicka = 0.9f;  // 0.1 spent * 2.0 = 0.2 over half an hour = 0.4 per hour
	Update(state, inputs, config);
	CHECK(state.rateActivity == doctest::Approx(1.6f));
	CHECK(state.rate == doctest::Approx(1.6f));  // the player: no NPC terms

	// regained stamina is not activity
	inputs.stamina = 1.0f;
	inputs.magicka = 1.0f;
	Update(state, inputs, config);
	CHECK(state.rateActivity == 0.0f);

	inputs.player = false;
	inputs.movement = Movement::Running;
	Update(state, inputs, config);
	CHECK(state.rate == doctest::Approx((0.5f + 1.0f) * 2.0f));
}

TEST_CASE("a forced wetness overrides the model and respects the cap")
{
	Config config;
	auto   state = Started(config);
	state.forceWetness = 7.0f;
	Update(state, Hour(10.0f), config);
	CHECK(state.wetness == 7.0f);
	CHECK(state.phase == Phase::Soaked);

	state.forceWetness = -1.0f;
	config.fWetnessForce = 50.0f;
	Update(state, Hour(), config);
	CHECK(state.wetness == config.fWetnessCap);
}

TEST_CASE("water wets by depth: waist-deep is soaked, swimming is the cap, ankle-deep is nothing")
{
	const auto config = Still();
	CHECK(WaterFloor(0.0f, config) == 0.0f);
	CHECK(WaterFloor(0.2f, config) == 0.0f);
	CHECK(WaterFloor(0.5f, config) == doctest::Approx(7.0f));
	CHECK(WaterFloor(0.7f, config) == config.fWetnessCap);
	CHECK(WaterFloor(1.0f, config) == config.fWetnessCap);

	auto state = Started(config);
	auto inputs = Hour(0.01f);
	inputs.submersion = 0.2f;
	Update(state, inputs, config);
	CHECK(state.wetness == 0.0f);

	// the least depth that counts already leaves drops
	inputs.submersion = config.fWaterMinDepth;
	Update(state, inputs, config);
	CHECK(state.phase == Phase::Soaked);
	CHECK(state.hasDrops);

	inputs.submersion = 0.5f;  // the complaint on 4453: waist-deep stayed dry
	Update(state, inputs, config);
	CHECK(state.wetness == doctest::Approx(7.0f));

	inputs.submersion = 1.0f;
	Update(state, inputs, config);
	CHECK(state.wetness == config.fWetnessCap);
}

TEST_CASE("standing in water keeps the wetness up while the weather dries, and does not count as sweat")
{
	Config config;  // indoors dries at 2 per hour
	auto   state = Started(config);
	auto   inputs = Hour(1.0f);
	inputs.submersion = 0.5f;
	for (int i = 0; i < 5; ++i) {
		Update(state, inputs, config);
		CHECK(state.wetness == doctest::Approx(7.0f));
	}
	CHECK(state.phase == Phase::Soaked);
	CHECK_FALSE(state.hasSweat);

	// out of the water it dries again
	inputs.submersion = 0.0f;
	Update(state, inputs, config);
	CHECK(state.wetness == doctest::Approx(5.0f));
	CHECK_FALSE(state.hasSweat);

	// back into deep water: the jump in wetness is water, not sweat
	inputs.submersion = 1.0f;
	Update(state, inputs, config);
	CHECK(state.wetness == config.fWetnessCap);
	CHECK_FALSE(state.hasSweat);
}

TEST_CASE("wading in and swimming never makes sweat, even while running through the water (seen in game 22.11)")
{
	const auto config = Still();
	auto       state = Started(config);
	auto       inputs = Hour(0.05f);
	inputs.movement = Movement::Running;  // the player runs by default, also through water
	// knee-deep, a while at the waist, deeper, swimming: the water floor steps up, running adds on top
	for (const float depth : { 0.32f, 0.35f, 0.45f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f,
							   0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f, 0.56f,
							   0.56f, 0.56f, 0.56f, 0.6f, 0.65f, 1.0f, 1.0f, 1.0f }) {
		inputs.submersion = depth;
		Update(state, inputs, config);
		CHECK_MESSAGE(!state.hasSweat, "depth ", depth, " wetness ", state.wetness);
	}
	CHECK(state.phase == Phase::Soaked);
	// out on the shore, standing: still no sweat
	inputs.submersion = 0.0f;
	inputs.movement = Movement::Still;
	Update(state, inputs, config);
	CHECK_FALSE(state.hasSweat);
}

TEST_CASE("late sweat: soaked by water, drying, then exertion brings sweat on top of the drops")
{
	const auto config = Still();
	auto       state = Started(config);
	auto       inputs = Hour(0.0f);
	inputs.submersion = 1.0f;
	Update(state, inputs, config);
	REQUIRE(state.phase == Phase::Soaked);
	REQUIRE_FALSE(state.hasSweat);

	inputs.submersion = 0.0f;
	state.wetness = 5.0f;  // dried to 5
	Update(state, inputs, config);
	CHECK_FALSE(state.hasSweat);

	state.wetness = 5.5f;  // a little more is not enough: strength +0.055 against fLateSweat 0.1
	Update(state, inputs, config);
	CHECK_FALSE(state.hasSweat);

	state.wetness = 6.5f;
	Update(state, inputs, config);
	CHECK(state.hasSweat);
	CHECK(state.hasDrops);
}

TEST_CASE("heat dries only a wet actor and only when enabled")
{
	auto config = Still();
	auto state = Started(config);
	auto inputs = Hour(0.5f);
	inputs.nearHeat = true;
	Update(state, inputs, config);
	CHECK(state.rateHeat == 0.0f);

	state.wetness = 6.0f;
	Update(state, inputs, config);
	CHECK(state.rateHeat == -4.0f);
	CHECK(state.wetness == doctest::Approx(4.0f));

	config.bHeatDrying = false;
	Update(state, inputs, config);
	CHECK(state.rateHeat == 0.0f);
}

TEST_CASE("arousal sweats but does not keep an actor soaked")
{
	const auto config = Still();
	auto       state = Started(config);
	auto       inputs = Hour(1.0f);
	inputs.arousal = 100;  // 1.5 per hour
	Update(state, inputs, config);
	CHECK(state.wetness == doctest::Approx(1.5f));

	state.wetness = 5.0f;  // above "soaked": arousal adds nothing
	Update(state, inputs, config);
	CHECK(state.rateArousal == 0.0f);
	CHECK(state.wetness == doctest::Approx(5.0f));
}

TEST_CASE("genital textures: quarter of the cap, arousal threshold, chastity")
{
	const auto config = Still();
	auto       state = Started(config);
	state.forceWetness = 2.0f;
	Update(state, Hour(), config);
	CHECK_FALSE(state.hasSchlong);
	CHECK_FALSE(state.hasPussy);

	state.forceWetness = 3.0f;
	Update(state, Hour(), config);
	CHECK(state.hasSchlong);
	CHECK(state.hasPussy);  // no arousal framework: follows the wetness

	auto inputs = Hour();
	inputs.arousal = 50;
	Update(state, inputs, config);
	CHECK_FALSE(state.hasPussy);
	inputs.arousal = 80;
	Update(state, inputs, config);
	CHECK(state.hasPussy);

	inputs.genitalsBlocked = true;
	Update(state, inputs, config);
	CHECK_FALSE(state.hasPussy);
	CHECK_FALSE(state.hasSchlong);
}

TEST_CASE("the first start grants the bonus once")
{
	Config config;
	config.fAutoBonusNormal = 1.0f;
	config.fAutoBonusRandom = 4.0f;
	config.fAutoBonusHours = 2.0f;
	State state;
	CHECK(Start(state, Inputs{}, config, 0.5f) == 2.0f);
	CHECK(state.active);
	CHECK(state.wetness == doctest::Approx(3.0f));

	state.wetness = 0.5f;
	state.phase = Phase::Soaked;
	state.hasDrops = true;
	CHECK(Start(state, Inputs{}, config, 1.0f) == 0.0f);
	CHECK(state.wetness == 0.5f);
	CHECK(state.phase == Phase::Dry);
	CHECK_FALSE(state.hasDrops);
}

TEST_CASE("forced specular and glossiness win over the model, per actor before the global")
{
	Config config;
	State  state;
	state.active = true;
	auto look = ComputeLook(state, config);
	CHECK(look.specular == -1.0f);
	CHECK(look.glossiness == -1.0f);

	config.fSpecularForce = 8.0f;
	config.fGlossinessForce = 500.0f;  // 4453: glossiness above 100 was asked for
	look = ComputeLook(state, config);
	CHECK(look.specular == 8.0f);
	CHECK(look.glossiness == 500.0f);

	state.forceSpecular = 20.0f;
	CHECK(ComputeLook(state, config).specular == 20.0f);
}

// ---------------------------------------------------------------------- textures
TEST_CASE("skin mode from the body: model space, UBE by race or by normal map, else floats")
{
	CHECK(ModeFor(false, true, "NordRace", "") == SkinMode::None);
	CHECK(ModeFor(true, true, "NordRace", "textures\\actors\\character\\female\\femalebody_1_msn.dds") == SkinMode::Specular);
	CHECK(ModeFor(true, false, "UBE_NordRace", "") == SkinMode::Normal);
	CHECK(ModeFor(true, false, "NordRace", "textures\\!UBE\\Body\\femalebody_1_n.dds") == SkinMode::Normal);
	CHECK(ModeFor(true, false, "NordRace", "textures\\actors\\character\\female\\femalebody_1.dds") == SkinMode::Floats);
	CHECK(TextureSlot(SkinMode::Specular) == 7);
	CHECK(TextureSlot(SkinMode::Normal) == 1);
}

TEST_CASE("a soaked sweaty aroused woman on a specular skin")
{
	Config     config;
	const Look look{ .active = true, .drops = true, .sweat = true, .pussy = true, .schlong = true, .specular = 9.0f, .glossiness = 30.0f };
	const auto plan = PlanLook(look, Skin{ .mode = SkinMode::Specular, .female = true }, config);
	CHECK(At(plan, Part::Body).texture == Tex("wet_111_s.dds"));
	CHECK(At(plan, Part::Feet).texture == Tex("wet_111_s.dds"));
	CHECK(At(plan, Part::Hands).texture == Tex("wethand_11_s.dds"));
	CHECK(At(plan, Part::Head).texture == Tex("wethead_11_s.dds"));
	CHECK(At(plan, Part::Head).fallback == Tex("wethead_s.dds"));
	CHECK(At(plan, Part::Schlong).texture.empty());  // no schlong mesh
	CHECK(At(plan, Part::Body).slot == 7);
	CHECK(At(plan, Part::Body).specular == 9.0f);
	CHECK(At(plan, Part::Head).specular == 9.0f);
	CHECK(At(plan, Part::Body).glossiness == -1.0f);  // glossiness is off by default
}

TEST_CASE("UBE takes the maps into the normal slot; men have their own two-digit set")
{
	Config     config;
	const Look look{ .active = true, .drops = true, .sweat = false, .pussy = false, .schlong = true, .specular = 5.0f };
	auto       plan = PlanLook(look, Skin{ .mode = SkinMode::Normal, .female = true }, config);
	CHECK(At(plan, Part::Body).texture == Tex("wet_100_UBE_n.dds"));
	CHECK(At(plan, Part::Head).texture == Tex("wethead_10_UBE_n.dds"));
	CHECK(At(plan, Part::Head).fallback == Tex("wethead_UBE_n.dds"));
	CHECK(At(plan, Part::Body).slot == 1);

	plan = PlanLook(look, Skin{ .mode = SkinMode::Specular, .female = false, .schlong = true }, config);
	CHECK(At(plan, Part::Body).texture == Tex("male_wet_10_s.dds"));
	CHECK(At(plan, Part::Hands).texture == Tex("male_wethand_10_s.dds"));
	CHECK(At(plan, Part::Hands).fallback == Tex("male_wethand_s.dds"));
	CHECK(At(plan, Part::Schlong).texture == Tex("male_wetschlong_s.dds"));
}

TEST_CASE("a futanari schlong is opt-in and keeps the specular map on a UBE body")
{
	Config     config;
	const Look look{ .active = true, .drops = true, .schlong = true };
	const Skin skin{ .mode = SkinMode::Normal, .female = true, .schlong = true };
	CHECK(At(PlanLook(look, skin, config), Part::Schlong).texture.empty());

	config.bTextureFutaSchlong = true;
	const auto plan = PlanLook(look, skin, config);
	CHECK(At(plan, Part::Schlong).texture == Tex("wetschlong_s.dds"));
	CHECK(At(plan, Part::Schlong).slot == 7);
	CHECK(At(plan, Part::Body).slot == 1);
}

TEST_CASE("the pussy map never shows alone, and feet follow their own switches")
{
	Config config;
	Look   look{ .active = true, .pussy = true, .specular = 4.0f };
	auto   plan = PlanLook(look, Skin{ .mode = SkinMode::Specular }, config);
	CHECK(At(plan, Part::Body).texture.empty());
	CHECK(At(plan, Part::Hands).texture.empty());
	CHECK(At(plan, Part::Body).specular == 4.0f);  // the shine still applies

	look.sweat = true;
	config.bTextureFeetSweat = false;
	plan = PlanLook(look, Skin{ .mode = SkinMode::Specular }, config);
	CHECK(At(plan, Part::Body).texture == Tex("wet_011_s.dds"));
	CHECK(At(plan, Part::Feet).texture.empty());

	config.bTextureBody = false;
	config.bTextureHead = false;
	plan = PlanLook(look, Skin{ .mode = SkinMode::Specular }, config);
	CHECK(At(plan, Part::Body).texture.empty());
	CHECK(At(plan, Part::Head).texture.empty());
	// the body map is off: the hands go by the look itself (sweat)
	CHECK(At(plan, Part::Hands).texture == Tex("wethand_01_s.dds"));
}

TEST_CASE("head and hands wear the map of the body's state, so the neck and the wrists meet the body map")
{
	Config config;
	const Skin skin{ .mode = SkinMode::Specular };
	// drops only: the body shows wet_100, the head must not come with the shinier sweat map (seen in game 22.11)
	auto plan = PlanLook(Look{ .active = true, .drops = true }, skin, config);
	CHECK(At(plan, Part::Body).texture == Tex("wet_100_s.dds"));
	CHECK(At(plan, Part::Head).texture == Tex("wethead_10_s.dds"));
	CHECK(At(plan, Part::Hands).texture == Tex("wethand_10_s.dds"));

	plan = PlanLook(Look{ .active = true, .sweat = true, .pussy = true }, skin, config);
	CHECK(At(plan, Part::Body).texture == Tex("wet_011_s.dds"));
	CHECK(At(plan, Part::Head).texture == Tex("wethead_01_s.dds"));

	// the body's sweat map is switched off: soaked and sweaty shows wet_100 on the body, so "10" on the head
	config.bTextureBodySweat = false;
	plan = PlanLook(Look{ .active = true, .drops = true, .sweat = true }, skin, config);
	CHECK(At(plan, Part::Body).texture == Tex("wet_100_s.dds"));
	CHECK(At(plan, Part::Head).texture == Tex("wethead_10_s.dds"));
	CHECK(At(plan, Part::Hands).fallback == Tex("wethand_s.dds"));
}

TEST_CASE("shine only, floats-only skins and inactive looks carry no textures")
{
	Config     config;
	const Look look{ .active = true, .drops = true, .sweat = true, .specular = 6.0f, .glossiness = 30.0f };
	config.bGlossinessBody = true;
	config.fHeadSpecularMult = 0.5f;
	config.fHandSpecularMult = 2.0f;

	for (const auto& skin : { Skin{ .mode = SkinMode::Specular, .textures = false }, Skin{ .mode = SkinMode::Floats } }) {
		const auto plan = PlanLook(look, skin, config);
		for (const auto& want : plan) {
			CHECK(want.texture.empty());
		}
		CHECK(At(plan, Part::Body).specular == 6.0f);
		CHECK(At(plan, Part::Body).glossiness == 30.0f);
		CHECK(At(plan, Part::Feet).glossiness == -1.0f);
		CHECK(At(plan, Part::Head).specular == 3.0f);
		CHECK(At(plan, Part::Hands).specular == 12.0f);
	}

	Look off = look;
	off.active = false;
	CHECK(PlanLook(off, Skin{ .mode = SkinMode::Specular }, config) == PlanLook(Look{}, Skin{ .mode = SkinMode::Specular }, config));
	for (const auto& want : PlanLook(off, Skin{ .mode = SkinMode::Specular }, config)) {
		CHECK(want.texture.empty());
		CHECK(want.specular == -1.0f);
	}
	for (const auto& want : PlanLook(look, Skin{ .mode = SkinMode::None }, config)) {
		CHECK(want.texture.empty());
		CHECK(want.specular == -1.0f);
	}
}

TEST_CASE("every planned texture is in the preload set")
{
	Config config;
	config.bTextureFutaSchlong = true;
	for (const bool female : { true, false }) {
		for (const auto mode : { SkinMode::Specular, SkinMode::Normal }) {
			const auto set = WetTextureSet(female, mode);
			for (int bits = 0; bits < 8; ++bits) {
				const Look look{ .active = true, .drops = (bits & 4) != 0, .sweat = (bits & 2) != 0, .pussy = (bits & 1) != 0, .schlong = true };
				for (const auto& want : PlanLook(look, Skin{ .mode = mode, .female = female, .schlong = true }, config)) {
					if (!want.texture.empty()) {
						// a per-state head or hand map is optional; the map it falls back to is not
						const auto& required = want.fallback.empty() ? want.texture : want.fallback;
						CHECK_MESSAGE(std::find(set.begin(), set.end(), required) != set.end(), required);
					}
				}
			}
		}
	}
	CHECK(WetTextureSet(true, SkinMode::Floats).empty());
}

TEST_CASE("our maps and the old WetFunction maps are told apart, whatever the slashes and case")
{
	CHECK(IsOurTexture(WetTexture(true, SkinMode::Specular, "wet_110")));
	CHECK(IsOurTexture("Textures/Actors/Character/drench/wethead_s.dds"));
	CHECK_FALSE(IsOurTexture("textures\\actors\\character\\WetFunction\\wethead_s.dds"));
	CHECK_FALSE(IsOurTexture("textures\\actors\\character\\female\\femalebody_1_s.dds"));
	CHECK_FALSE(IsOurTexture(""));
	CHECK(IsLegacyTexture("textures\\actors\\character\\WetFunction\\wethead_s.dds"));
	CHECK(IsLegacyTexture("data/textures/actors/character/wetfunction/male_wet_11_s.dds"));
	CHECK_FALSE(IsLegacyTexture(WetTexture(true, SkinMode::Normal, "wethand")));
}

// ---------------------------------------------------------------------- actor modes
TEST_CASE("wet maps by texture mode and actor mode: UBE can keep its own skin normal (4453, Meemaan41)")
{
	const auto all = static_cast<std::int32_t>(TextureMode::All);
	const auto noUBE = static_cast<std::int32_t>(TextureMode::ShineOnlyUBE);
	const auto none = static_cast<std::int32_t>(TextureMode::ShineOnly);
	CHECK(WantsTextures(SkinMode::Specular, ActorMode::Normal, all));
	CHECK(WantsTextures(SkinMode::Normal, ActorMode::Normal, all));
	CHECK(WantsTextures(SkinMode::Specular, ActorMode::Normal, noUBE));
	CHECK_FALSE(WantsTextures(SkinMode::Normal, ActorMode::Normal, noUBE));
	CHECK_FALSE(WantsTextures(SkinMode::Specular, ActorMode::Normal, none));
	CHECK_FALSE(WantsTextures(SkinMode::Normal, ActorMode::Normal, none));
	CHECK_FALSE(WantsTextures(SkinMode::Specular, ActorMode::ShineOnly, all));
	CHECK_FALSE(WantsTextures(SkinMode::Specular, ActorMode::Excluded, all));
	CHECK_FALSE(WantsTextures(SkinMode::Floats, ActorMode::Normal, all));
	CHECK_FALSE(WantsTextures(SkinMode::None, ActorMode::Normal, all));
	CHECK(WantsTextures(SkinMode::Normal, ActorMode::Normal, 77));  // a value the INI should not hold: the default, all
}

TEST_CASE("shine only keeps the specular and glossiness but takes no map")
{
	Config     config;
	config.bGlossinessBody = true;
	const Look look{ .active = true, .drops = true, .sweat = true, .specular = 9.0f, .glossiness = 40.0f };
	const auto plan = PlanLook(look, Skin{ .mode = SkinMode::Normal, .textures = WantsTextures(SkinMode::Normal, ActorMode::ShineOnly, 0) }, config);
	for (const auto& want : plan) {
		CHECK(want.texture.empty());
	}
	CHECK(At(plan, Part::Body).specular == 9.0f);
	CHECK(At(plan, Part::Body).glossiness == 40.0f);
}

TEST_CASE("actor mode names round-trip and tolerate case and spelling")
{
	for (const auto mode : { ActorMode::Normal, ActorMode::ShineOnly, ActorMode::Excluded }) {
		CHECK(ParseActorMode(ActorModeName(mode)) == mode);
	}
	CHECK(ParseActorMode("Excluded") == ActorMode::Excluded);
	CHECK(ParseActorMode("OFF") == ActorMode::Excluded);
	CHECK(ParseActorMode("Shine Only") == ActorMode::ShineOnly);
	CHECK_FALSE(ParseActorMode("wet"));
	CHECK_FALSE(ParseActorMode(""));
}

TEST_CASE("actor keys: plugin and local id, the player, pasted full form ids")
{
	const ActorKey lydia{ "Skyrim.esm", 0x0A2C94 };
	CHECK(FormatActorKey(lydia) == "Skyrim.esm|0x0A2C94");
	CHECK(ParseActorKey("Skyrim.esm|0x0A2C94") == lydia);
	CHECK(ParseActorKey("  Skyrim.esm | 0a2c94 ") == lydia);
	CHECK(ParseActorKey("Skyrim.esm|0x000A2C94") == lydia);
	CHECK(ParseActorKey("Skyrim.esm|0xFE0A2C94")->local == 0x0A2C94);  // the load-order byte is dropped
	CHECK(FormatActorKey(ActorKey{}) == "player");
	CHECK(ParseActorKey("Player") == ActorKey{});
	CHECK_FALSE(ParseActorKey("Skyrim.esm"));
	CHECK_FALSE(ParseActorKey("|0x123"));
	CHECK_FALSE(ParseActorKey("Skyrim.esm|0xZZ"));
	CHECK_FALSE(ParseActorKey("Skyrim.esm|0x123456789"));
}

// ---------------------------------------------------------------------- co-save
TEST_CASE("the co-save keeps what the model needs and nothing runtime")
{
	State wet;
	wet.wetness = 6.25f;
	wet.forceWetness = 3.0f;
	wet.forceSpecular = 14.0f;
	wet.forceGlossiness = 600.0f;
	wet.autoStop = 123.456789;
	wet.active = true;
	wet.manual = true;
	wet.seeded = true;
	wet.phase = Phase::Soaked;
	wet.hasDrops = true;
	wet.hasSweat = true;
	wet.dryDyn = 1.0f;
	wet.maxDyn = 3.5f;
	wet.recentStrength = 0.4f;
	wet.rate = 9.0f;      // runtime
	wet.sexRate = 5.0f;   // runtime
	State sweaty;
	sweaty.wetness = 2.5f;
	sweaty.phase = Phase::Sweat;
	sweaty.hasSweat = true;

	const std::vector<SavedActor> saved{ { 0x14, wet }, { 0xFF000ABC, sweaty } };
	const auto                    loaded = Decode(Encode(saved));
	REQUIRE(loaded);
	REQUIRE(loaded->size() == 2);
	const auto& [id, state] = loaded->front();
	CHECK(id == 0x14);
	CHECK(state.wetness == 6.25f);
	CHECK(state.forceWetness == 3.0f);
	CHECK(state.forceSpecular == 14.0f);
	CHECK(state.forceGlossiness == 600.0f);
	CHECK(state.autoStop == 123.456789);
	CHECK(state.active);
	CHECK(state.manual);
	CHECK(state.seeded);
	CHECK(state.phase == Phase::Soaked);
	CHECK(state.hasDrops);
	CHECK(state.hasSweat);
	CHECK(state.dryDyn == 1.0f);
	CHECK(state.maxDyn == 3.5f);
	CHECK(state.recentStrength == 0.4f);
	CHECK(state.rate == 0.0f);
	CHECK(state.sexRate == 0.0f);
	CHECK(loaded->back().first == 0xFF000ABC);
	CHECK(loaded->back().second.phase == Phase::Sweat);
	CHECK_FALSE(loaded->back().second.active);
}

TEST_CASE("an empty save round-trips, and only actors with something to remember are worth saving")
{
	const auto loaded = Decode(Encode({}));
	REQUIRE(loaded);
	CHECK(loaded->empty());

	State state;
	CHECK_FALSE(WorthSaving(state));
	state.seeded = true;
	CHECK_FALSE(WorthSaving(state));
	state.wetness = 0.1f;
	CHECK(WorthSaving(state));
	state = {};
	state.forceWetness = 0.0f;  // forced dry is a choice to keep
	CHECK(WorthSaving(state));
	state = {};
	state.active = true;
	CHECK(WorthSaving(state));
}

TEST_CASE("damaged or truncated co-save data is refused, not half-read")
{
	State state;
	state.wetness = 5.0f;
	const std::vector<SavedActor> saved{ { 0x14, state }, { 0x15, state } };
	auto                          data = Encode(saved);

	CHECK_FALSE(Decode({}));
	CHECK_FALSE(Decode(std::span(data).first(3)));
	CHECK_FALSE(Decode(std::span(data).first(data.size() - 1)));

	auto count = data;
	count[4] = 200;  // claims more records than the data holds
	CHECK_FALSE(Decode(count));

	auto size = data;
	size[2] = 4;  // a record smaller than the known layout
	size[3] = 0;
	CHECK_FALSE(Decode(size));
}

TEST_CASE("a newer save with longer records is read up to the fields this version knows")
{
	State state;
	state.wetness = 5.0f;
	state.active = true;
	state.recentStrength = 0.75f;
	const auto current = Encode(std::vector<SavedActor>{ { 0x14, state }, { 0x15, state } });
	const auto recordSize = static_cast<std::size_t>(current[2] | (current[3] << 8));

	std::vector<std::uint8_t> newer(current.begin(), current.begin() + 8);
	newer[0] = 2;
	newer[2] = static_cast<std::uint8_t>(recordSize + 6);
	for (std::size_t i = 0; i < 2; ++i) {
		const auto begin = current.begin() + 8 + static_cast<std::ptrdiff_t>(i * recordSize);
		newer.insert(newer.end(), begin, begin + static_cast<std::ptrdiff_t>(recordSize));
		newer.insert(newer.end(), 6, std::uint8_t{ 0xEE });
	}
	const auto loaded = Decode(newer);
	REQUIRE(loaded);
	REQUIRE(loaded->size() == 2);
	CHECK(loaded->back().first == 0x15);
	CHECK(loaded->back().second.wetness == 5.0f);
	CHECK(loaded->back().second.recentStrength == 0.75f);
}

TEST_CASE("NaN, a bad phase and a zero form id from a damaged save never reach the model")
{
	State bad;
	bad.wetness = std::numeric_limits<float>::quiet_NaN();
	bad.forceWetness = std::numeric_limits<float>::infinity();
	bad.dryDyn = std::numeric_limits<float>::quiet_NaN();
	bad.autoStop = std::numeric_limits<double>::quiet_NaN();
	bad.active = true;
	auto data = Encode(std::vector<SavedActor>{ { 0x14, bad }, { 0, bad } });
	// the phase byte of the first record: header 8, id 4, four floats 16, autoStop 8, flags 1
	data[8 + 4 + 16 + 8 + 1] = 77;

	const auto loaded = Decode(data);
	REQUIRE(loaded);
	REQUIRE(loaded->size() == 1);
	const auto& state = loaded->front().second;
	CHECK(state.wetness == 0.0f);
	CHECK(state.forceWetness == -1.0f);
	CHECK(state.dryDyn == 0.0f);
	CHECK(state.autoStop == 0.0);
	CHECK(state.phase == Phase::Soaked);
	CHECK(state.active);
}

TEST_CASE("a Goam ear wears the ear map made for the head map it sits on")
{
	const std::string head = R"(textures\actors\character\Drench\)";
	const std::string goam = R"(textures\Goam\GoamNewElvenEars\)";
	CHECK(EarTexture(head + "wethead_10_s.dds", goam + "GoamEarSm_s.dds") == head + "wetear_GoamSm_10_s.dds");
	CHECK(EarTexture(head + "wethead_11_UBE_n.dds", goam + "GoamEarLrg_s.dds") == head + "wetear_GoamLrg_11_UBE_s.dds");
	CHECK(EarTexture(head + "wethead_s.dds", goam + "goamearxl_s.dds") == head + "wetear_GoamXL_s.dds");
	CHECK(EarTexture(head + "male_wethead_UBE_n.dds", goam + "GoamEarXL_s.dds") == head + "male_wetear_GoamXL_UBE_s.dds");
	CHECK(EarTexture(head + "male_wethead_s.dds", R"(TEXTURES\GOAM\GOAMNEWELVENEARS\GOAMEARSM_S.DDS)") == head + "male_wetear_GoamSm_s.dds");
}

TEST_CASE("a COTR face is told by its geometry: model-space normals and a COTR vertex count")
{
	CHECK(IsCotrHead(12206, true));
	CHECK(IsCotrHead(3127, true));
	CHECK_FALSE(IsCotrHead(12206, false));  // a tangent-space head of that size is not COTR
	CHECK_FALSE(IsCotrHead(3832, true));    // HPH
	CHECK_FALSE(IsCotrHead(996, true));     // vanilla
	CHECK_FALSE(IsCotrHead(11498, false));  // UBE
}

TEST_CASE("a COTR face wears the COTR map of the state planned for the body, always a specular one")
{
	const std::string dir = R"(textures\actors\character\Drench\)";
	const auto maps = [&](const std::string& a_head) { return CotrHeadTextures(dir + a_head); };
	CHECK(maps("wethead_10_s.dds").texture == dir + "wethead_COTR_10_s.dds");
	CHECK(maps("wethead_10_s.dds").fallback == dir + "wethead_COTR_s.dds");
	CHECK(maps("wethead_11_UBE_n.dds").texture == dir + "wethead_COTR_11_s.dds");  // UBE body, COTR head
	CHECK(maps("WETHEAD_01_S.DDS").texture == dir + "wethead_COTR_01_s.dds");
	CHECK(maps("wethead_s.dds").texture == dir + "wethead_COTR_s.dds");
	CHECK(maps("wethead_s.dds").fallback.empty());
	CHECK(maps("male_wethead_s.dds").texture == dir + "male_wethead_COTR_s.dds");
	CHECK(maps("male_wethead_UBE_n.dds").texture == dir + "male_wethead_COTR_s.dds");
	CHECK(maps("male_wethead_s.dds").fallback.empty());
	CHECK(maps("wethand_10_s.dds").texture.empty());
	CHECK(maps("wethead_10.dds").texture.empty());
	CHECK(CotrHeadTextures("").texture.empty());
}

TEST_CASE("other head parts, an unplanned head and unknown ears get no ear map")
{
	const std::string head = R"(textures\actors\character\Drench\wethead_10_s.dds)";
	const std::string sm = R"(textures\Goam\GoamNewElvenEars\GoamEarSm_s.dds)";
	CHECK(EarTexture("", sm).empty());                                                                    // head not painted
	CHECK(EarTexture(head, "").empty());                                                                  // no own specular
	CHECK(EarTexture(head, R"(textures\actors\character\female\femalehead_s.dds)").empty());              // another part
	CHECK(EarTexture(head, R"(textures\Goam\GoamNewElvenEars\GoamEarHuge_s.dds)").empty());               // unknown size
	CHECK(EarTexture(head, R"(textures\Goam\GoamNewElvenEars\GoamEarSm_msn.dds)").empty());               // not the _s
	CHECK(EarTexture(R"(textures\actors\character\Drench\wethand_10_s.dds)", sm).empty());                // not a head map
}

TEST_CASE("a wet 3BA body map has a detail map of the same state for Mu Dynamic NormalMap")
{
	const std::string folder = R"(textures\actors\character\Drench\)";
	CHECK(DetailTexture(folder + "wet_110_s.dds") == folder + "wet_110_detail_n.dds");
	CHECK(DetailTexture(folder + "wet_011_s.dds") == folder + "wet_011_detail_n.dds");
	CHECK(DetailTexture(folder + "WET_101_S.DDS") == folder + "wet_101_detail_n.dds");
	CHECK(DetailTexture("").empty());                                  // dry body
	CHECK(DetailTexture(folder + "wet_110_UBE_n.dds").empty());        // UBE: the relief is in its own map
	CHECK(DetailTexture(folder + "male_wet_11_s.dds").empty());        // no male detail maps
	CHECK(DetailTexture(folder + "wethead_10_s.dds").empty());         // only the body
	CHECK(DetailTexture(folder + "wet_001_s.dds").empty());            // never planned, no map
}

// ---------------------------------------------------------------------- drips (the embedded DripSim)
TEST_CASE("a sweating actor gets running sweat drops that grow from the sweat start to soaked")
{
	const auto config = Still();
	auto       state = Started(config);
	auto       inputs = Hour();
	inputs.movement = Movement::Sprinting;  // +1 per hour

	for (int i = 0; i < 2; ++i) {
		Update(state, inputs, config);
	}
	CHECK(DripHeat(state, config) == 0.0f);  // 2.0: dry phase, nothing shows

	Update(state, inputs, config);  // 3.0: sweating, halfway to soaked
	REQUIRE(state.hasSweat);
	const float half = DripHeat(state, config);
	CHECK(half > 0.3f);
	CHECK(half < 1.0f);

	Update(state, inputs, config);  // 4.0: soaked with sweat
	Update(state, inputs, config);
	CHECK(DripHeat(state, config) == doctest::Approx(1.0f));
	CHECK(DripHeat(state, config) > half);
}

TEST_CASE("water on the skin is not sweat, and the drips can be switched off")
{
	const auto config = Still();
	auto       state = Started(config);
	auto       inputs = Hour(0.1f);
	inputs.submersion = 1.0f;  // a swim
	Update(state, inputs, config);
	REQUIRE(state.hasDrops);
	CHECK_FALSE(state.hasSweat);
	CHECK(DripHeat(state, config) == 0.0f);

	auto sweating = Started(config);
	sweating.wetness = 4.0f;
	sweating.hasSweat = true;
	sweating.phase = Phase::Soaked;
	auto off = config;
	off.bDripsSweat = false;
	CHECK(DripHeat(sweating, off) == 0.0f);
	off = config;
	off.bDrips = false;
	CHECK(DripHeat(sweating, off) == 0.0f);
	CHECK(DripHeat(sweating, config) > 0.0f);
}

TEST_CASE("a bath or a script that soaks the actor pours water over the skin, the deeper the more")
{
	const Config config;  // dry 1, soaked 4, cap 10
	CHECK(DripSoak(0.0f, 8.0f, config) == doctest::Approx(0.8f));   // a bath from dry
	CHECK(DripSoak(0.0f, 10.0f, config) == doctest::Approx(1.0f));  // to the cap
	CHECK(DripSoak(0.0f, 4.0f, config) == doctest::Approx(0.4f));   // just soaked
	CHECK(DripSoak(0.0f, 3.0f, config) == 0.0f);                    // only sweaty: not water
	CHECK(DripSoak(7.5f, 8.0f, config) == 0.0f);                    // already that wet: no new water
	CHECK(DripSoak(8.0f, 2.0f, config) == 0.0f);                    // drying it
	auto off = config;
	off.bDripsWater = false;
	CHECK(DripSoak(0.0f, 8.0f, off) == 0.0f);
	off = config;
	off.bDrips = false;
	CHECK(DripSoak(0.0f, 8.0f, off) == 0.0f);
}

TEST_CASE("a skin patch's own wet maps are looked up by that skin's normal map")
{
	// the LRO cellulite skin: its patch ships wet maps under skins\<its normal path>
	CHECK(SkinWetTexture(Tex("wet_110_UBE_n.dds"), R"(Textures\!UBE\Body\femalebody_1_n.dds)") ==
	      Tex(R"(skins\!ube\body\femalebody_1_n\wet_110_UBE_n.dds)"));
	// Babette's own skin is another folder, so with no maps there she keeps the shared ones
	CHECK(SkinWetTexture(Tex("wethand_10_UBE_n.dds"), "textures/Eidolon/Babette/Skin/Body/femalebody_1_n.dds") ==
	      Tex(R"(skins\eidolon\babette\skin\body\femalebody_1_n\wethand_10_UBE_n.dds)"));
	// a path given with Data\ or without textures\ means the same file
	CHECK(SkinWetTexture(Tex("wet_001_UBE_n.dds"), R"(Data\textures\!UBE\Body\femalebody_1_n.dds)") ==
	      Tex(R"(skins\!ube\body\femalebody_1_n\wet_001_UBE_n.dds)"));
	CHECK(SkinWetTexture(Tex("wet_001_UBE_n.dds"), R"(!UBE\Body\femalebody_1_n.dds)") ==
	      Tex(R"(skins\!ube\body\femalebody_1_n\wet_001_UBE_n.dds)"));
	// nothing to look up
	CHECK(SkinWetTexture(Tex("wet_110_UBE_n.dds"), "").empty());
	CHECK(SkinWetTexture("", R"(Textures\!UBE\Body\femalebody_1_n.dds)").empty());
	CHECK(SkinWetTexture(R"(textures\actors\character\female\femalebody_1_n.dds)", R"(Textures\!UBE\Body\femalebody_1_n.dds)").empty());
	CHECK(SkinWetTexture(Tex("wet_110_UBE_n.dds"), Tex("wet_001_UBE_n.dds")).empty());
}

TEST_CASE("a character's skin choice picks the wet maps: auto, shared or a forced skin")
{
	constexpr auto lro = R"(Textures\!UBE\Body\femalebody_1_n.dds)";
	constexpr auto babette = R"(Textures\Eidolon\Babette\Skin\Body\femalebody_1_n.dds)";
	CHECK(SkinKey(lro) == R"(!ube\body\femalebody_1_n)");
	CHECK(SkinKey(babette) == R"(eidolon\babette\skin\body\femalebody_1_n)");
	CHECK(SkinKey("").empty());
	CHECK(SkinKey(Tex("wet_001_UBE_n.dds")).empty());

	const auto wet = Tex("wet_110_UBE_n.dds");
	const auto lroMap = Tex(R"(skins\!ube\body\femalebody_1_n\wet_110_UBE_n.dds)");
	// auto: by the skin the character wears
	CHECK(ChosenSkinWetTexture(wet, lro, "") == lroMap);
	CHECK(ChosenSkinWetTexture(wet, babette, "") == Tex(R"(skins\eidolon\babette\skin\body\femalebody_1_n\wet_110_UBE_n.dds)"));
	// forced: Babette in her own skin gets the LRO maps
	CHECK(ChosenSkinWetTexture(wet, babette, R"(!ube\body\femalebody_1_n)") == lroMap);
	// a forced key written with other case or slashes still names the same folder
	CHECK(ChosenSkinWetTexture(wet, babette, "!UBE/Body/femalebody_1_n") == lroMap);
	// shared: nothing skin-specific, even for the LRO skin
	CHECK(ChosenSkinWetTexture(wet, lro, kSharedSkin).empty());
	CHECK(ChosenSkinWetTexture(wet, lro, "Shared").empty());
	// not our map: nothing
	CHECK(ChosenSkinWetTexture(R"(textures\actors\character\female\femalebody_1_n.dds)", lro, "").empty());
}
