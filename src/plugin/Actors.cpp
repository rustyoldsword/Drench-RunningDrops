#include "Actors.h"

namespace Drench::Actors
{
	namespace
	{
		constexpr auto kFile = "Data/SKSE/Plugins/Drench_Actors.ini"sv;

		std::mutex                                 g_lock;
		std::vector<Entry>                         g_entries;  // file order, for the menu
		std::unordered_map<RE::FormID, ActorMode>  g_byForm;   // resolved in this load order
		std::vector<SkinEntry>                     g_skins;    // [Skins], file order
		std::unordered_map<RE::FormID, std::string> g_skinByForm;

		RE::FormID Resolve(const ActorKey& a_key)
		{
			if (a_key.plugin.empty()) {
				return 0x14;
			}
			auto* data = RE::TESDataHandler::GetSingleton();
			return data ? data->LookupFormID(a_key.local, a_key.plugin) : 0;
		}

		// the base record that stands for this character in every save
		RE::TESNPC* StableBase(RE::Actor* a_actor)
		{
			auto* npc = a_actor->GetActorBase();
			if (npc && npc->IsDynamicForm()) {
				npc = a_actor->GetTemplateBase();
			}
			return npc && !npc->IsDynamicForm() ? npc : nullptr;
		}

		void Save()
		{
			const auto      path = std::filesystem::path(kFile);
			std::error_code ec;
			std::filesystem::create_directories(path.parent_path(), ec);
			std::ofstream out(path, std::ios::trunc);
			if (!out) {
				logger::error("Could not write {}", path.string());
				return;
			}
			out << "; Drench - characters that are not wetted normally. One line per character:\n"
				   ";   <plugin>|<form id of the character's base record> = shine | excluded\n"
				   "; shine: the skin keeps its own maps, only the shine changes; excluded: never wet.\n"
				   "; 'player' is the player. Set them in the Drench menu (Actors) or with the console: drench shine / exclude / normal.\n\n"
				   "[Actors]\n";
			for (const auto& entry : g_entries) {
				out << FormatActorKey(entry.key) << " = " << ActorModeName(entry.mode) << '\n';
			}
			out << "\n; Skin wet maps per character: <plugin>|<form id> = shared | <skin folder under textures\\actors\\character\\Drench\\skins>\n"
				   "; shared: only Drench's shared maps; a folder: that skin's maps whatever skin the character wears.\n"
				   "; Not listed: chosen by the character's own skin. Set them in the Drench menu (Characters).\n\n"
				   "[Skins]\n";
			for (const auto& entry : g_skins) {
				out << FormatActorKey(entry.key) << " = " << entry.choice << '\n';
			}
		}
	}

	void Load()
	{
		std::scoped_lock lock(g_lock);
		g_entries.clear();
		g_byForm.clear();
		g_skins.clear();
		g_skinByForm.clear();
		std::ifstream in{ std::filesystem::path(kFile) };
		std::string   line;
		std::size_t   unresolved = 0;
		bool          skins = false;  // inside [Skins]
		while (in && std::getline(in, line)) {
			if (const auto comment = line.find_first_of(";#"); comment != std::string::npos) {
				line.erase(comment);
			}
			if (const auto open = line.find('['); open != std::string::npos && line.find('=') == std::string::npos) {
				std::string section(line.substr(open));
				std::ranges::transform(section, section.begin(), [](char a_char) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); });
				skins = section.starts_with("[skins]");
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = ParseActorKey(std::string_view(line).substr(0, eq));
			auto       value = std::string_view(line).substr(eq + 1);
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
				value.remove_prefix(1);
			}
			while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
				value.remove_suffix(1);
			}
			if (skins) {
				if (!key || value.empty()) {
					logger::warn("{}: cannot read '{}'", kFile, line);
					continue;
				}
				g_skins.push_back({ *key, std::string(value) });
				if (const auto id = Resolve(*key)) {
					g_skinByForm[id] = std::string(value);
				}
				continue;
			}
			const auto mode = ParseActorMode(value);
			if (!key || !mode) {
				logger::warn("{}: cannot read '{}'", kFile, line);
				continue;
			}
			if (*mode == ActorMode::Normal) {
				continue;
			}
			g_entries.push_back({ *key, *mode });
			if (const auto id = Resolve(*key)) {
				g_byForm[id] = *mode;
			} else {
				++unresolved;  // the plugin is not in this load order: kept in the file, ignored
			}
		}
		logger::info("Actor modes: {} from {}{}", g_entries.size(), kFile, unresolved ? std::format(" ({} for plugins not loaded)", unresolved) : ""s);
		if (!g_skins.empty()) {
			logger::info("Skin wet map choices: {}", g_skins.size());
		}
	}

	std::optional<ActorKey> KeyOf(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return std::nullopt;
		}
		if (a_actor->IsPlayerRef()) {
			return ActorKey{};
		}
		auto* base = StableBase(a_actor);
		auto* file = base ? base->GetFile(0) : nullptr;
		if (!file) {
			return std::nullopt;
		}
		return ActorKey{ std::string(file->GetFilename()), base->GetLocalFormID() };
	}

	ActorMode Mode(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return ActorMode::Normal;
		}
		std::scoped_lock lock(g_lock);
		if (g_byForm.empty()) {
			return ActorMode::Normal;
		}
		const RE::FormID id = a_actor->IsPlayerRef() ? 0x14 : [&] {
			auto* base = StableBase(a_actor);
			return base ? base->GetFormID() : 0u;
		}();
		const auto it = g_byForm.find(id);
		return it != g_byForm.end() ? it->second : ActorMode::Normal;
	}

	bool SetMode(const ActorKey& a_key, ActorMode a_mode)
	{
		std::scoped_lock lock(g_lock);
		std::erase_if(g_entries, [&](const Entry& a_entry) { return a_entry.key == a_key; });
		const auto id = Resolve(a_key);
		g_byForm.erase(id);
		if (a_mode != ActorMode::Normal) {
			g_entries.push_back({ a_key, a_mode });
			if (id) {
				g_byForm[id] = a_mode;
			}
		}
		Save();
		logger::info("Actor mode: {} -> {}", FormatActorKey(a_key), ActorModeName(a_mode));
		return true;
	}

	bool SetMode(RE::Actor* a_actor, ActorMode a_mode)
	{
		const auto key = KeyOf(a_actor);
		if (!key) {
			logger::warn("{}: no stable base record, the mode cannot be remembered", a_actor ? a_actor->GetName() : "none");
			return false;
		}
		return SetMode(*key, a_mode);
	}

	std::string Describe(const ActorKey& a_key)
	{
		if (a_key.plugin.empty()) {
			return "Player";
		}
		const auto id = Resolve(a_key);
		const auto* form = id ? RE::TESForm::LookupByID(id) : nullptr;
		const char* name = form ? form->GetName() : nullptr;
		return name && *name ? std::format("{} ({})", name, FormatActorKey(a_key)) : FormatActorKey(a_key);
	}

	std::vector<Entry> All()
	{
		std::scoped_lock lock(g_lock);
		return g_entries;
	}

	std::string SkinChoice(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return {};
		}
		std::scoped_lock lock(g_lock);
		if (g_skinByForm.empty()) {
			return {};
		}
		const RE::FormID id = a_actor->IsPlayerRef() ? 0x14 : [&] {
			auto* base = StableBase(a_actor);
			return base ? base->GetFormID() : 0u;
		}();
		const auto it = g_skinByForm.find(id);
		return it != g_skinByForm.end() ? it->second : std::string();
	}

	bool SetSkinChoice(const ActorKey& a_key, std::string a_choice)
	{
		std::scoped_lock lock(g_lock);
		std::erase_if(g_skins, [&](const SkinEntry& a_entry) { return a_entry.key == a_key; });
		const auto id = Resolve(a_key);
		g_skinByForm.erase(id);
		if (!a_choice.empty()) {
			g_skins.push_back({ a_key, a_choice });
			if (id) {
				g_skinByForm[id] = a_choice;
			}
		}
		Save();
		logger::info("Skin wet maps: {} -> {}", FormatActorKey(a_key), a_choice.empty() ? "auto"s : a_choice);
		return true;
	}

	bool SetSkinChoice(RE::Actor* a_actor, std::string a_choice)
	{
		const auto key = KeyOf(a_actor);
		if (!key) {
			logger::warn("{}: no stable base record, the skin choice cannot be remembered", a_actor ? a_actor->GetName() : "none");
			return false;
		}
		return SetSkinChoice(*key, std::move(a_choice));
	}

	std::vector<SkinEntry> AllSkins()
	{
		std::scoped_lock lock(g_lock);
		return g_skins;
	}
}
