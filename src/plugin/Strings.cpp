#include "Strings.h"

namespace Drench::Strings
{
	namespace
	{
		std::once_flag                               g_once;
		std::string                                  g_language;
		std::unordered_map<std::string, std::string> g_text;  // English -> translation; never changes after Load

		std::string Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r\n");
			return std::string(a_text.substr(first, last - first + 1));
		}

		std::string GameLanguage()
		{
			auto* ini = RE::INISettingCollection::GetSingleton();
			auto* setting = ini ? ini->GetSetting("sLanguage:General") : nullptr;
			const char* value = setting && setting->GetType() == RE::Setting::Type::kString ? setting->GetString() : nullptr;
			std::string language = value ? value : "";
			std::ranges::transform(language, language.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			return language;
		}

		// "\n" in the file stands for a line break, so a long help text stays on one line
		std::string Unescape(std::string a_text)
		{
			for (std::size_t at = a_text.find("\\n"); at != std::string::npos; at = a_text.find("\\n", at + 1)) {
				a_text.replace(at, 2, "\n");
			}
			return a_text;
		}
	}

	void Load()
	{
		// Tried before the menu pages are named and again at data loaded: the game's INI may not be read on the
		// first try, and only a known language is final.
		if (GameLanguage().empty()) {
			return;
		}
		std::call_once(g_once, [] {
			g_language = GameLanguage();
			if (g_language == "ENGLISH") {
				logger::info("Menu language: English");
				return;
			}
			const auto    path = std::filesystem::path(std::format("Data/SKSE/Plugins/Drench/Strings_{}.ini", g_language));
			std::ifstream in(path);
			std::string   line;
			while (in && std::getline(in, line)) {
				if (line.empty() || line.front() == ';' || line.front() == '#' || line.front() == '[') {
					continue;
				}
				// split at the first " = ": English texts never hold one, a translation may
				const auto eq = line.find(" = ");
				if (eq == std::string::npos) {
					continue;
				}
				auto english = Unescape(Trim(std::string_view(line).substr(0, eq)));
				auto translation = Unescape(Trim(std::string_view(line).substr(eq + 3)));
				if (!english.empty() && !translation.empty()) {
					g_text.insert_or_assign(std::move(english), std::move(translation));
				}
			}
			logger::info("Menu language: {} ({} texts from {})", g_language, g_text.size(), in ? path.string() : "no file, English shown"s);
		});
	}

	const char* T(const char* a_english)
	{
		if (g_text.empty() || !a_english) {
			return a_english;
		}
		const auto it = g_text.find(a_english);
		return it != g_text.end() ? it->second.c_str() : a_english;
	}

	std::string Language() { return g_language; }
	std::size_t Count() { return g_text.size(); }
}
