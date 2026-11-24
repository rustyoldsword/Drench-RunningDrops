#include "Heat.h"

namespace Drench::Heat
{
	namespace
	{
		constexpr auto kSunHelm = "SunHelmSurvival.esp"sv;
		constexpr auto kSurvival = "ccqdrsse001-survivalmode.esl"sv;
		constexpr RE::FormID kSurvivalWarmUpList = 0x8AA;  // Survival_WarmUpObjectsList: the fires Survival Mode warms the player at

		// local FormIDs in SunHelmSurvival.esp: heat source lists and the global that holds each list's radius
		struct SunHelmList
		{
			RE::FormID list;
			RE::FormID radius;
			float      fallback;
		};
		constexpr SunHelmList kSmall{ 0x78511B, 0x743404, 250.0f };
		constexpr SunHelmList kNormal{ 0x78511A, 0x743403, 450.0f };
		constexpr SunHelmList kLarge{ 0x785119, 0x78A21C, 600.0f };
		constexpr SunHelmList kAll{ 0x785118, 0x743403, 450.0f };  // sources that sit in no size list

		constexpr std::array kHeatModels{ "campfire"sv, "fireplace"sv, "firepit"sv, "firebowl"sv, "brazier"sv, "hearth"sv, "bonfire"sv, "forge"sv };
		constexpr std::array kColdModels{ "burnt"sv, "burned"sv, "unlit"sv, "ashes"sv, "dead"sv, "extinguish"sv, "_off"sv };

		std::unordered_map<RE::FormID, float> g_sunHelm;  // base object -> reach
		std::unordered_set<RE::FormID>        g_survival;  // base objects from Survival Mode's warm-up list
		std::unordered_map<RE::FormID, bool>  g_models;   // base object -> looks like a fire
		float                                 g_sunHelmReach{ 0.0f };
		std::atomic_size_t                    g_sunHelmCount{ 0 };
		std::atomic_size_t                    g_survivalCount{ 0 };

		bool Contains(std::string_view a_text, std::string_view a_part)
		{
			return a_text.find(a_part) != std::string_view::npos;
		}

		bool LooksLikeFire(const RE::TESBoundObject* a_base)
		{
			if (!a_base->Is(RE::FormType::Activator, RE::FormType::Furniture, RE::FormType::Static, RE::FormType::MovableStatic)) {
				return false;
			}
			const auto* model = skyrim_cast<const RE::TESModel*>(a_base);
			const char* path = model ? model->GetModel() : nullptr;
			if (!path || !*path) {
				return false;
			}
			std::string name(path);
			std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const auto has = [&](std::string_view a_part) { return Contains(name, a_part); };
			return std::ranges::any_of(kHeatModels, has) && !std::ranges::any_of(kColdModels, has);
		}

		// how far the heat of a base object reaches; 0 means it is not a fire
		float Reach(const RE::TESBoundObject* a_base, bool a_sunHelm, bool a_survival, const Settings& a_settings)
		{
			if (a_sunHelm) {
				if (const auto it = g_sunHelm.find(a_base->GetFormID()); it != g_sunHelm.end()) {
					return it->second;
				}
			}
			if (a_survival && g_survival.contains(a_base->GetFormID())) {
				return a_settings.fHeatRange;
			}
			auto it = g_models.find(a_base->GetFormID());
			if (it == g_models.end()) {
				it = g_models.emplace(a_base->GetFormID(), LooksLikeFire(a_base)).first;
			}
			return it->second ? a_settings.fHeatRange : 0.0f;
		}

		void Load(RE::TESDataHandler* a_data, const SunHelmList& a_source, bool a_keepExisting)
		{
			auto* list = a_data->LookupForm<RE::BGSListForm>(a_source.list, kSunHelm);
			if (!list) {
				return;
			}
			const auto* global = a_data->LookupForm<RE::TESGlobal>(a_source.radius, kSunHelm);
			const float reach = global ? global->value : a_source.fallback;
			g_sunHelmReach = std::max(g_sunHelmReach, reach);
			list->ForEachForm([&](RE::TESForm* a_form) {
				if (a_form) {
					if (a_keepExisting) {
						g_sunHelm.emplace(a_form->GetFormID(), reach);
					} else {
						g_sunHelm[a_form->GetFormID()] = reach;
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
	}

	void Refresh()
	{
		Forget();  // every game load: the cells of the old game are gone
		g_sunHelm.clear();
		g_sunHelmReach = 0.0f;
		auto* data = RE::TESDataHandler::GetSingleton();
		if (data && data->LookupModByName(kSunHelm)) {
			Load(data, kSmall, false);
			Load(data, kNormal, false);
			Load(data, kLarge, false);
			Load(data, kAll, true);
		}
		g_sunHelmCount = g_sunHelm.size();

		// Survival Mode lists its fires without radii; its warm-up check uses its own range, ours stays fHeatRange
		g_survival.clear();
		if (auto* list = data ? data->LookupForm<RE::BGSListForm>(kSurvivalWarmUpList, kSurvival) : nullptr) {
			list->ForEachForm([](RE::TESForm* a_form) {
				if (a_form) {
					g_survival.insert(a_form->GetFormID());
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}
		g_survivalCount = g_survival.size();
		logger::info("Heat sources: SunHelm {} ({} base objects), Survival Mode {} ({} base objects)", g_sunHelmCount ? "found" : "absent", g_sunHelmCount.load(),
			g_survivalCount ? "found" : "absent", g_survivalCount.load());
	}

	std::size_t SunHelmSources()
	{
		return g_sunHelmCount;
	}

	std::size_t SurvivalSources()
	{
		return g_survivalCount;
	}

	namespace
	{
		// The fires of one cell, found once and reused: every update of every wet actor walked all references of
		// its cell before. Rebuilt when the cell changes, the settings change, or after kFireListAge.
		struct Fire
		{
			RE::ObjectRefHandle ref;
			RE::NiPoint3        position;
			float               reach;
		};
		struct CellFires
		{
			std::vector<Fire>                     fires;
			std::chrono::steady_clock::time_point built;
			std::uint32_t                         settings{ 0 };
		};
		constexpr auto                                kFireListAge = 30s;
		std::unordered_map<RE::TESObjectCELL*, CellFires> g_cells;

		std::uint32_t SettingsKey(const Settings& a_settings)
		{
			std::uint32_t range;
			std::memcpy(std::addressof(range), std::addressof(a_settings.fHeatRange), sizeof(range));
			return range ^ (a_settings.bHeatSunHelm ? 0x40000000u : 0u) ^ (a_settings.bHeatSurvivalMode ? 0x20000000u : 0u);
		}

		const std::vector<Fire>& FiresOf(RE::TESObjectCELL* a_cell, const Settings& a_settings)
		{
			const auto now = std::chrono::steady_clock::now();
			const auto key = SettingsKey(a_settings);
			auto&      entry = g_cells[a_cell];
			if (entry.built.time_since_epoch().count() != 0 && now - entry.built < kFireListAge && entry.settings == key) {
				return entry.fires;
			}
			if (g_cells.size() > 64) {  // cells come and go; keep the few the player is around
				std::erase_if(g_cells, [&](const auto& a_item) { return a_item.first != a_cell && now - a_item.second.built > kFireListAge; });
			}
			const bool sunHelm = a_settings.bHeatSunHelm && !g_sunHelm.empty();
			const bool survival = a_settings.bHeatSurvivalMode && !g_survival.empty();
			entry.fires.clear();
			a_cell->ForEachReference([&](RE::TESObjectREFR* a_ref) {
				const auto* base = a_ref ? a_ref->GetBaseObject() : nullptr;
				if (base && !base->Is(RE::FormType::NPC)) {
					if (const float reach = Reach(base, sunHelm, survival, a_settings); reach > 0.0f) {
						entry.fires.push_back({ a_ref->GetHandle(), a_ref->GetPosition(), reach });
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
			entry.built = now;
			entry.settings = key;
			return entry.fires;
		}
	}

	bool IsNear(RE::Actor* a_actor, const Settings& a_settings)
	{
		auto* cell = a_settings.bHeatDrying ? a_actor->GetParentCell() : nullptr;
		if (!cell) {
			return false;
		}
		const auto position = a_actor->GetPosition();
		for (const auto& fire : FiresOf(cell, a_settings)) {
			if (fire.position.GetDistance(position) > fire.reach) {
				continue;
			}
			// a fire put out or not drawn does not warm
			const auto ref = fire.ref.get();
			if (ref && !ref->IsDisabled() && ref->Is3DLoaded()) {
				return true;
			}
		}
		return false;
	}

	void Forget()
	{
		g_cells.clear();
	}
}
