#include "Settings.h"

#include "Status.h"

namespace Drench
{
	namespace
	{
		constexpr auto kSettings = "Data/SKSE/Plugins/Drench.ini"sv;
		// read once, when Drench has no settings of its own yet: the same keys under the predecessor's name
		constexpr auto kPredecessor = "Data/SKSE/Plugins/WetFunctionNG.ini"sv;

		std::string Trim(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r\n");
			return std::string(a_text.substr(first, last - first + 1));
		}

		bool IEquals(std::string_view a_lhs, std::string_view a_rhs)
		{
			return a_lhs.size() == a_rhs.size() && std::equal(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), [](char a, char b) {
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			});
		}

		void Parse(const std::string& a_value, float& a_out)
		{
			try {
				a_out = std::stof(a_value);
			} catch (...) {
			}
		}

		void Parse(const std::string& a_value, bool& a_out)
		{
			a_out = a_value == "1" || IEquals(a_value, "true");
		}

		void Parse(const std::string& a_value, std::int32_t& a_out)
		{
			try {
				a_out = std::stoi(a_value);
			} catch (...) {
			}
		}

		std::string Value(float a_value) { return std::format("{:.6g}", a_value); }
		std::string Value(bool a_value) { return a_value ? "1" : "0"; }
		std::string Value(std::int32_t a_value) { return std::to_string(a_value); }

		std::size_t ReadIni(std::string_view a_path, Settings& a_settings)
		{
			std::ifstream in{ std::filesystem::path(a_path) };
			if (!in) {
				return 0;
			}
			std::size_t applied = 0;
			std::string section;
			std::string line;
			while (std::getline(in, line)) {
				if (const auto comment = line.find_first_of(";#"); comment != std::string::npos) {
					line.erase(comment);
				}
				line = Trim(line);
				if (line.empty()) {
					continue;
				}
				if (line.front() == '[' && line.back() == ']') {
					section = Trim(std::string_view(line).substr(1, line.size() - 2));
					continue;
				}
				const auto eq = line.find('=');
				if (eq == std::string::npos) {
					continue;
				}
				if (a_settings.Set(section, Trim(std::string_view(line).substr(0, eq)), Trim(std::string_view(line).substr(eq + 1)))) {
					++applied;
				}
			}
			return applied;
		}
	}

	Settings& Settings::Get()
	{
		static Settings singleton;
		return singleton;
	}

	bool Settings::Set(std::string_view a_section, std::string_view a_key, const std::string& a_value)
	{
#define DRENCH_SET(a_sec, a_name, a_default)                                  \
	if (IEquals(a_section, #a_sec) && IEquals(a_key, #a_name)) {            \
		Parse(a_value, a_name);                                             \
		return true;                                                        \
	}
		DRENCH_FLOAT_SETTINGS(DRENCH_SET)
		DRENCH_BOOL_SETTINGS(DRENCH_SET)
		DRENCH_INT_SETTINGS(DRENCH_SET)
#undef DRENCH_SET
		return false;
	}

	void Settings::Load()
	{
		Settings        fresh;
		std::error_code ec;
		std::size_t     carried = 0;
		if (!std::filesystem::exists(std::filesystem::path(kSettings), ec)) {
			if ((carried = ReadIni(kPredecessor, fresh)) > 0) {
				logger::info("No Drench.ini yet: {} values taken from WetFunctionNG.ini", carried);
			}
		}
		const auto values = ReadIni(kSettings, fresh);
		Get() = fresh;
		spdlog::set_level(fresh.iLogLevel > 0 ? spdlog::level::debug : spdlog::level::info);
		logger::info("Settings loaded: {} values", values);
		Status::settingsSource = values ? std::format("Drench.ini, {} values", values) :
		                         carried ? std::format("WetFunctionNG.ini, {} values (Drench.ini is written on the first change in the menu)", carried) :
		                                   "built-in defaults (no Drench.ini yet)"s;
	}

	bool Settings::Save()
	{
		const auto path = std::filesystem::path(kSettings);
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);
		std::ofstream out(path, std::ios::trunc);
		if (!out) {
			logger::error("Could not save settings to {}", path.string());
			return false;
		}

		std::map<std::string, std::vector<std::pair<std::string, std::string>>> sections;
		const auto& settings = Get();
#define DRENCH_COLLECT(a_section, a_name, a_default) sections[#a_section].emplace_back(#a_name, Value(settings.a_name));
		DRENCH_FLOAT_SETTINGS(DRENCH_COLLECT)
		DRENCH_BOOL_SETTINGS(DRENCH_COLLECT)
		DRENCH_INT_SETTINGS(DRENCH_COLLECT)
#undef DRENCH_COLLECT

		out << "; Drench - changed in SKSE Menu Framework\n";
		for (const auto& [section, entries] : sections) {
			out << '\n' << '[' << section << "]\n";
			for (const auto& [key, value] : entries) {
				out << key << " = " << value << '\n';
			}
		}
		out.flush();
		if (!out) {
			logger::error("Could not finish writing settings to {}", path.string());
			return false;
		}
		spdlog::set_level(settings.iLogLevel > 0 ? spdlog::level::debug : spdlog::level::info);
		return true;
	}

	bool Settings::Reset()
	{
		Get() = Settings{};
		return Save();
	}
}
