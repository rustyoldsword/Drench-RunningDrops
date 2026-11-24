#include "Wetness.h"

#include "Arousal.h"
#include "Devious.h"
#include "Heat.h"

namespace Drench::Wetness
{
	namespace
	{
		// wetfunctionfurniture10 (WetFunction.esp): anvils, grindstones, wood chopping, smelters, tanning racks...
		const std::unordered_set<RE::FormID> kWorkFurniture{
			0x0001A2AD, 0x000D932F, 0x000CAE0B, 0x000BBCF1, 0x000BF9E1, 0x0006E9C2, 0x0009C6CE, 0x000727A1, 0x0009C6DF, 0x000BD15D,
			0x000F9AF8, 0x000BD15C, 0x000613A6, 0x000613A7, 0x000E2BC7, 0x000F9ACC, 0x00071C47, 0x0007022E, 0x0010F575, 0x000A91BF
		};

		float Percent(RE::Actor* a_actor, RE::ActorValue a_value)
		{
			auto*       owner = a_actor->AsActorValueOwner();
			const float max = owner->GetPermanentActorValue(a_value);
			return max > 0.0f ? std::clamp(owner->GetActorValue(a_value) / max, 0.0f, 1.0f) : 1.0f;
		}

		// Weather.GetClassification: pleasant, cloudy, rainy, snow, none
		Weather Classify(const RE::TESWeather* a_weather, bool a_interior)
		{
			if (a_interior) {
				return Weather::Indoors;
			}
			if (!a_weather) {
				return Weather::None;
			}
			using Flag = RE::TESWeather::WeatherDataFlag;
			const auto flags = a_weather->data.flags;
			if (flags.any(Flag::kPleasant)) {
				return Weather::Pleasant;
			}
			if (flags.any(Flag::kCloudy)) {
				return Weather::Cloudy;
			}
			if (flags.any(Flag::kRainy)) {
				return Weather::Rain;
			}
			if (flags.any(Flag::kSnow)) {
				return Weather::Snow;
			}
			return Weather::Unclassified;
		}

		bool IsWorking(RE::Actor* a_actor)
		{
			if (a_actor->AsActorState()->GetSitSleepState() != RE::SIT_SLEEP_STATE::kIsSitting) {
				return false;
			}
			const auto  furniture = a_actor->GetOccupiedFurniture().get();
			const auto* base = furniture ? furniture->GetBaseObject() : nullptr;
			return base && kWorkFurniture.contains(base->GetFormID());
		}

		// 0..1 of the actor's height under water
		float Submersion(RE::Actor* a_actor)
		{
			// the swimming flag alone was set on the player during a new game start
			if (a_actor->AsActorState()->IsSwimming() && a_actor->IsInWater()) {
				return 1.0f;
			}
			auto* cell = a_actor->GetParentCell();
			if (!cell) {
				return 0.0f;
			}
			return std::clamp(a_actor->GetSubmergeLevel(a_actor->GetPositionZ(), cell), 0.0f, 1.0f);
		}

		Inputs Read(RE::Actor* a_actor, const ActorRecord& a_record, const Settings& a_settings)
		{
			Inputs inputs;
			inputs.player = a_actor->IsPlayerRef();
			inputs.stamina = Percent(a_actor, RE::ActorValue::kStamina);
			inputs.magicka = Percent(a_actor, RE::ActorValue::kMagicka);
			if (a_actor->AsActorState()->IsSprinting()) {
				inputs.movement = a_actor->IsOnMount() ? Movement::Galloping : Movement::Sprinting;
			} else if (a_actor->IsRunning()) {
				inputs.movement = Movement::Running;
			} else if (a_actor->IsSneaking()) {
				inputs.movement = Movement::Sneaking;
			}
			inputs.working = IsWorking(a_actor);

			auto*      cell = a_actor->GetParentCell();
			const bool interior = cell && cell->IsInteriorCell();
			if (auto* sky = RE::Sky::GetSingleton(); sky && !interior) {
				inputs.weather = Classify(sky->currentWeather, false);
				inputs.outgoingWeather = Classify(sky->lastWeather, false);
				inputs.weatherPct = std::clamp(sky->currentWeatherPct, 0.0f, 1.0f);
			} else {
				inputs.weather = inputs.outgoingWeather = interior ? Weather::Indoors : Weather::None;
			}

			inputs.submersion = Submersion(a_actor);
			inputs.arousal = Arousal::Get(a_actor);
			inputs.genitalsBlocked = a_settings.bDeviousBlockGenitals && Devious::BlocksGenitals(a_actor);
			// a fire dries an actor faster; nothing to dry, nothing to look up
			inputs.nearHeat = a_settings.bHeatDrying && a_record.wetness > 0.0f && Heat::IsNear(a_actor, a_settings);
			return inputs;
		}
	}

	float GameDays()
	{
		auto* calendar = RE::Calendar::GetSingleton();
		return calendar ? calendar->GetCurrentGameTime() : 0.0f;
	}

	void Start(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings)
	{
		static std::mt19937 rng{ std::random_device{}() };
		Inputs              inputs;
		inputs.stamina = Percent(a_actor, RE::ActorValue::kStamina);
		inputs.magicka = Percent(a_actor, RE::ActorValue::kMagicka);
		a_record.headStart = Drench::Start(a_record, inputs, a_settings, std::uniform_real_distribution<float>(0.0f, 1.0f)(rng));
		a_record.lastGameTime = GameDays();
		a_record.nextUpdate = {};
	}

	void Update(RE::Actor* a_actor, ActorRecord& a_record, const Settings& a_settings)
	{
		const float now = GameDays();
		auto        inputs = Read(a_actor, a_record, a_settings);
		inputs.hours = (a_record.lastGameTime >= 0.0f ? std::max(0.0f, (now - a_record.lastGameTime) * 24.0f) : 0.0f) + a_record.headStart;
		a_record.lastGameTime = now;
		a_record.headStart = 0.0f;
		Drench::Update(a_record, inputs, a_settings);
	}
}
