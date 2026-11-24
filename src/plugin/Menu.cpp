#include "Menu.h"

#include "Actors.h"
#include "Devious.h"
#include "Heat.h"
#include "Manager.h"
#include "Mdnm.h"
#include "OStim.h"
#include "Settings.h"
#include "Status.h"
#include "Strings.h"
#include "Visuals.h"
#include "drip/Drips.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

// Every text shown goes through T(): Data/SKSE/Plugins/Drench/Strings_<LANGUAGE>.ini translates it.
// tools/strings_template.py collects them into Strings_TEMPLATE.ini; keep each one a single string literal.
namespace Drench::Menu
{
	namespace
	{
		using Strings::T;

		void Commit()
		{
			Settings::Save();
			Manager::Get().OnSettingsChanged();
		}

		void Help(const char* a_text)
		{
			if (a_text && ImGuiMCP::IsItemHovered()) {
				ImGuiMCP::SetTooltip("%s", T(a_text));
			}
		}

		void Toggle(const char* a_label, bool& a_value, const char* a_help = nullptr)
		{
			if (ImGuiMCP::Checkbox(T(a_label), std::addressof(a_value))) {
				Commit();
			}
			Help(a_help);
		}

		void Slider(const char* a_label, float& a_value, float a_min, float a_max, const char* a_format, const char* a_help = nullptr)
		{
			if (ImGuiMCP::SliderFloat(T(a_label), std::addressof(a_value), a_min, a_max, a_format)) {
				Commit();
			}
			Help(a_help);
		}

		void Heading(const char* a_text)
		{
			ImGuiMCP::SeparatorText(T(a_text));
		}

		void Wrapped(const char* a_text)
		{
			ImGuiMCP::TextWrapped("%s", T(a_text));
		}

		bool Button(const char* a_label)
		{
			return ImGuiMCP::Button(T(a_label));
		}

		// the actor in front of the player, or the one picked in the console
		RE::Actor* PickedActor()
		{
			if (auto* pick = RE::CrosshairPickData::GetSingleton()) {
				if (const auto ref = pick->GetActiveTarget().get(); ref && ref->As<RE::Actor>()) {
					return ref->As<RE::Actor>();
				}
			}
			if (const auto ref = RE::Console::GetSelectedRef(); ref && ref->As<RE::Actor>()) {
				return ref->As<RE::Actor>();
			}
			return nullptr;
		}

		void __stdcall General()
		{
			auto& s = Settings::Get();
			Heading("Actors and update rate");
			Toggle("Enable player", s.bPlayerEnabled, "The player is always managed while this is enabled.");
			Toggle("Update first-person model", s.bFirstPerson);
			Slider("Player update interval", s.fLoopTimePC, 0.25f, 30.0f, "%.2f s");
			Slider("NPC update interval", s.fLoopTimeNPC, 0.25f, 60.0f, "%.2f s");
			Slider("Forced-value interval", s.fLoopTimeForced, 0.25f, 60.0f, "%.2f s");

			Heading("Logging");
			bool verbose = s.iLogLevel > 0;
			if (ImGuiMCP::Checkbox(T("Per-actor debug logging"), std::addressof(verbose))) {
				s.iLogLevel = verbose ? 1 : 0;
				Commit();
			}
			Help("Off by default. Lifecycle, warnings and errors are still logged; actor updates are not.");

			Heading("Maintenance");
			if (Button("Reload INI")) {
				Settings::Load();
				Manager::Get().OnSettingsChanged();
			}
			ImGuiMCP::SameLine();
			if (Button("Reset defaults")) {
				Settings::Reset();
				Manager::Get().OnSettingsChanged();
			}
			if (Button("Stop all effects")) {
				Manager::Get().StopAll(false);
			}
			ImGuiMCP::SameLine();
			if (Button("Clear all saved data")) {
				Manager::Get().StopAll(true);
			}
			if (Button("Remove old WetFunction overrides from the player")) {
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					const auto removed = Visuals::CleanLegacy(player, true);
					Visuals::Out(std::format("[Drench] Player: removed {} leftover wetness overrides", removed));
				}
			}
			Help("Wet Function Redux, WetFunction NG and Dewpoint 1.0 stored their wet look in RaceMenu's overrides, which stay in the save. Drench removes them from every actor it wets; this also takes the shine overrides they left.");
			Wrapped("Settings: Data\\SKSE\\Plugins\\Drench.ini");
		}

		void __stdcall Wetness()
		{
			auto& s = Settings::Get();
			Heading("Wetness points");
			Slider("Dry threshold", s.fWetnessDry, 0.0f, 30.0f, "%.1f");
			Slider("Visible sweat threshold", s.fWetnessStart, 0.0f, 30.0f, "%.1f");
			Slider("Soaked threshold", s.fWetnessSoaked, 0.0f, 30.0f, "%.1f");
			Slider("Maximum wetness", s.fWetnessCap, 0.1f, 30.0f, "%.1f");
			Slider("Force wetness", s.fWetnessForce, -1.0f, 30.0f, "%.1f", "-1 releases the forced value.");
			Slider("Late sweat ratio", s.fLateSweat, 0.0f, 1.0f, "%.2f");

			Heading("Water");
			Slider("Wet from this depth", s.fWaterMinDepth, 0.05f, 1.0f, "%.2f", "Share of the actor's height under water. Below it, wading leaves the actor dry.");
			Slider("Soaked through from this depth", s.fWaterFullDepth, 0.05f, 1.0f, "%.2f", "From this share of the height under water, and while swimming, the actor is as wet as can be.");

			Heading("Initial automatic effect");
			Slider("Simulated previous hours", s.fAutoBonusHours, 0.0f, 24.0f, "%.1f h");
			Slider("Flat starting wetness", s.fAutoBonusNormal, 0.0f, 30.0f, "%.1f");
			Slider("Random starting wetness", s.fAutoBonusRandom, 0.0f, 30.0f, "%.1f");
		}

		void __stdcall Sources()
		{
			auto& s = Settings::Get();
			Heading("Activity per game hour");
			Slider("Stamina use", s.fGenerateStamina, -10.0f, 10.0f, "%.2f");
			Slider("Magicka use", s.fGenerateMagicka, -10.0f, 10.0f, "%.2f");
			Slider("Sprinting", s.fGenerateSprinting, -10.0f, 10.0f, "%.2f");
			Slider("Running", s.fGenerateRunning, -10.0f, 10.0f, "%.2f");
			Slider("Sneaking", s.fGenerateSneaking, -10.0f, 10.0f, "%.2f");
			Slider("Mounted gallop", s.fGenerateGallop, -10.0f, 10.0f, "%.2f");
			Slider("Working furniture", s.fGenerateWorking, -10.0f, 10.0f, "%.2f");
			Slider("NPC additive modifier", s.fOtherAdd, -10.0f, 10.0f, "%.2f");
			Slider("NPC multiplier", s.fOtherMult, 0.0f, 10.0f, "%.2f");
			Slider("Global multiplier", s.fMultGlobal, 0.0f, 10.0f, "%.2f");

			Heading("Weather per game hour");
			Slider("Pleasant", s.fWeatherPleasant, -10.0f, 10.0f, "%.2f");
			Slider("Cloudy", s.fWeatherCloudy, -10.0f, 10.0f, "%.2f");
			Slider("Rain", s.fWeatherRainy, -10.0f, 10.0f, "%.2f");
			Slider("Snow", s.fWeatherSnow, -10.0f, 10.0f, "%.2f");
			Slider("Interior / none", s.fWeatherNone, -10.0f, 10.0f, "%.2f");

			Heading("Drying near a fire");
			Toggle("Dry faster near a fire", s.bHeatDrying, "Campfires, fireplaces, braziers and forges. Works for the player and every managed NPC.");
			Slider("Extra drying per hour", s.fHeatDrying, 0.0f, 20.0f, "%.2f");
			Slider("Fire reach", s.fHeatRange, 50.0f, 1500.0f, "%.0f", "Game units. SunHelm heat sources keep the radius SunHelm gives them.");
			Toggle("Use SunHelm heat sources", s.bHeatSunHelm, "Takes SunHelm's lists of fires and their radii when SunHelm Survival is installed.");
			Toggle("Use Survival Mode heat sources", s.bHeatSurvivalMode, "Takes the fires Creation Club Survival Mode warms you at; they use the fire reach above.");
			if (Heat::SurvivalSources() > 0) {
				ImGuiMCP::Text("%s: %zu", T("Survival Mode heat sources"), Heat::SurvivalSources());
			}
			if (Heat::SunHelmSources() > 0) {
				ImGuiMCP::Text("%s: %zu", T("SunHelm heat sources"), Heat::SunHelmSources());
			} else {
				Wrapped("SunHelm: not detected, fires are recognised by model name");
			}

			Heading("Arousal");
			Slider("Arousal generation", s.fGenerateArousal, -10.0f, 10.0f, "%.2f");
			Toggle("Use arousal threshold for pussy texture", s.bUseArousalThreshold);
			Slider("Arousal threshold", s.fArousedThreshold, 0.0f, 100.0f, "%.0f");
		}

		void __stdcall Integrations()
		{
			auto& s = Settings::Get();
			const auto detected = [](bool a_found) {
				ImGuiMCP::Text("%s: %s", T("Detected"), a_found ? T("yes") : T("no"));
			};
			Heading("SexLab");
			detected(Manager::Get().SexLabAvailable());
			Toggle("Enable SexLab", s.bSexLabEnabled);
			Toggle("SexLab: player scenes only", s.bSexLabPlayerOnly);
			Toggle("SexLab: auto-manage scene actors", s.bSexLabAuto, "All participants bypass the ordinary NPC sex/race filters.");
			Toggle("SexLab auto only when player participates", s.bSexLabAutoPlayer);
			Slider("SexLab base generation", s.fSexLabBase, -10.0f, 20.0f, "%.2f");
			Slider("SexLab enjoyment generation", s.fSexLabEnjoyment, -10.0f, 20.0f, "%.2f");
			Slider("SexLab pain generation", s.fSexLabPain, -10.0f, 20.0f, "%.2f");
			Slider("SexLab orgasm bonus", s.fSexLabOrgasm, -10.0f, 20.0f, "%.2f");
			Slider("SexLab linger", s.fSexLabAutoLinger, 0.0f, 48.0f, "%.1f h");

			Heading("OStim Standalone");
			detected(OStim::IsAvailable());
			Toggle("Enable OStim", s.bOStimEnabled);
			Toggle("OStim: player scenes only", s.bOStimPlayerOnly);
			Toggle("OStim: auto-manage scene actors", s.bOStimAuto, "All participants bypass the ordinary NPC sex/race filters.");
			Toggle("OStim auto only when player participates", s.bOStimAutoPlayer);
			Slider("OStim base generation", s.fOStimBase, -10.0f, 20.0f, "%.2f");
			Slider("OStim excitement generation", s.fOStimExcitement, -10.0f, 20.0f, "%.2f");
			Slider("OStim orgasm bonus", s.fOStimOrgasm, -10.0f, 20.0f, "%.2f");
			Slider("OStim linger", s.fOStimAutoLinger, 0.0f, 48.0f, "%.1f h");

			Heading("Devious Devices");
			detected(Devious::IsAvailable());
			Wrapped("Native part only for now: worn-device detection and chastity. Vibration, orgasm and edge from a device are not wired up yet - that lives in Devious Devices' own Papyrus scripts, not its native API.");
			Toggle("Chastity blocks the wet crotch/schlong texture", s.bDeviousBlockGenitals, "A belt or cage physically covers what the texture would show.");

			Heading("Bathing in Skyrim");
			Toggle("Wet after washing", s.bBathingSoak, "Washing with Bathing in Skyrim leaves the actor wet, also from a tub or with soap away from water.");
			Slider("Wetness after washing", s.fBathingWetness, 0.0f, 30.0f, "%.1f", "Raised to this value, never lowered. Capped by the maximum wetness.");
			Slider("NPC drying window", s.fBathingDuration, 0.0f, 24.0f, "%.1f h", "How long a washed NPC stays managed so you can watch them dry.");

			Heading("Running drops (DripSim)");
			Wrapped("Player only. Water and sweat drops run down the skin along the live pose. Needs RaceMenu and a free body overlay slot. Fine tuning: SKSE\\Plugins\\Drench\\Drips*.ini.");
			if (const char* off = Drips::Status(); *off) {
				Wrapped(off);
			}
			Toggle("Running drops", s.bDrips);
			Toggle("Sweat drops", s.bDripsSweat, "Follow Drench's sweat: they start with the sweat phase and grow until soaked.");
			Toggle("Water drops", s.bDripsWater, "Rain under open sky, leaving deep water, washing, and scripts that soak the player.");
		}

		void __stdcall Automatic()
		{
			auto& s = Settings::Get();
			Wrapped("Default: followers are managed; unrelated NPCs are ignored. Sex scene participants are controlled on the Integrations page and always bypass these filters.");
			Heading("Who is scanned");
			Toggle("Followers", s.bAutoFollower, "Player teammates of either sex are always eligible.");
			Toggle("All nearby NPCs", s.bAutoGlobal, "Off by default. Enables weather, activity and arousal wetness for ordinary NPCs.");
			Toggle("Nearby naked NPCs", s.bAutoNaked, "Off by default.");
			Toggle("NPCs wearing a Devious Devices device", s.bAutoDevious, "Requires Devious Devices NG.");
			Toggle("Female ordinary NPCs", s.bAutoFemale);
			Toggle("Male ordinary NPCs", s.bAutoMale);
			Toggle("Beast-race ordinary NPCs", s.bAutoBeast);
			Toggle("Vampire ordinary NPCs", s.bAutoVampire);
			Toggle("Keep wetness after auto-stop", s.bAutoKeepWetness);

			Heading("Scan");
			Slider("Scan interval", s.fAutoTimeout, 0.25f, 60.0f, "%.1f s");
			Slider("Scan radius", s.fAutoRange, 100.0f, 20000.0f, "%.0f");
			Slider("Automatic duration", s.fAutoDuration, 0.1f, 24.0f, "%.1f h");
		}

		// Skin folders that ship wet maps under textures\actors\character\Drench\skins (keys as core SkinKey makes them).
		// Read once; "Rescan skins" reads again after a skin patch is installed.
		std::vector<std::string>& SkinFolders(bool a_rescan = false)
		{
			static std::vector<std::string> folders;
			static bool                     scanned = false;
			if (scanned && !a_rescan) {
				return folders;
			}
			scanned = true;
			folders.clear();
			const std::filesystem::path root("Data/textures/actors/character/Drench/skins");
			std::error_code             ec;
			for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
				if (!it->is_regular_file(ec) || it->path().extension() != ".dds") {
					continue;
				}
				auto key = std::filesystem::relative(it->path().parent_path(), root, ec).string();
				std::ranges::transform(key, key.begin(), [](char a_char) {
					return a_char == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(a_char)));
				});
				if (!key.empty() && std::ranges::find(folders, key) == folders.end()) {
					folders.push_back(std::move(key));
				}
			}
			std::ranges::sort(folders);
			return folders;
		}

		// A combo for one character's skin wet maps: Auto, Shared, then every skin folder. Returns the new choice
		// when it changed.
		std::optional<std::string> SkinCombo(const char* a_label, const std::string& a_choice)
		{
			const auto&              folders = SkinFolders();
			std::vector<std::string> values{ "", std::string(kSharedSkin) };
			values.insert(values.end(), folders.begin(), folders.end());
			std::string lower(a_choice);
			std::ranges::transform(lower, lower.begin(), [](char a_char) {
				return a_char == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(a_char)));
			});
			int current = 0;
			if (const auto it = std::ranges::find(values, lower); it != values.end()) {
				current = static_cast<int>(it - values.begin());
			} else {
				values.push_back(lower);  // a folder that is not installed now: still shown, so it can be changed
				current = static_cast<int>(values.size() - 1);
			}
			std::vector<std::string> labels{ T("Auto (by own skin)"), T("Shared maps only") };
			labels.insert(labels.end(), values.begin() + 2, values.end());
			std::vector<const char*> items;
			for (const auto& label : labels) {
				items.push_back(label.c_str());
			}
			if (ImGuiMCP::Combo(a_label, std::addressof(current), items.data(), static_cast<int>(items.size()))) {
				return values[static_cast<std::size_t>(current)];
			}
			return std::nullopt;
		}

		void __stdcall Characters()
		{
			Wrapped("Characters that are wetted differently, in every save and new game. Shine only: the skin keeps its own maps and only shines when wet. Excluded: never wet, never touched. Every copy of a character shares the choice.");
			Wrapped("Console: aim at a character (or select it with the console) and type drench shine, drench exclude or drench normal.");

			static const char* const modes[]{ "Normal", "Shine only", "Excluded" };
			const char*              names[]{ T(modes[0]), T(modes[1]), T(modes[2]) };

			Heading("Add a character");
			const auto offer = [&](RE::Actor* a_actor, const char* a_label) {
				if (!a_actor) {
					return;
				}
				ImGuiMCP::PushID(a_label);
				int current = static_cast<int>(Actors::Mode(a_actor));
				ImGuiMCP::Text("%s: %s", T(a_label), a_actor->GetName());
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Combo("##mode", std::addressof(current), names, 3)) {
					if (!Actors::SetMode(a_actor, static_cast<ActorMode>(current))) {
						Visuals::Out(std::format("[Drench] {}: no lasting record to remember", a_actor->GetName()));
					}
				}
				ImGuiMCP::Text("  %s", T("Skin wet maps"));
				ImGuiMCP::SameLine();
				if (const auto choice = SkinCombo("##skin", Actors::SkinChoice(a_actor))) {
					if (!Actors::SetSkinChoice(a_actor, *choice)) {
						Visuals::Out(std::format("[Drench] {}: no lasting record to remember", a_actor->GetName()));
					}
				}
				ImGuiMCP::PopID();
			};
			offer(RE::PlayerCharacter::GetSingleton(), "Player");
			if (auto* picked = PickedActor(); picked && !picked->IsPlayerRef()) {
				offer(picked, "Under the crosshair / selected");
			} else {
				Wrapped("Look at a character before opening this menu (or select one in the console) to set them here.");
			}

			Heading("Saved characters");
			const auto entries = Actors::All();
			if (entries.empty()) {
				Wrapped("None: everyone is wetted normally.");
			}
			for (std::size_t i = 0; i < entries.size(); ++i) {
				const auto& entry = entries[i];
				ImGuiMCP::PushID(static_cast<int>(i));
				int current = static_cast<int>(entry.mode);
				if (ImGuiMCP::Combo(Actors::Describe(entry.key).c_str(), std::addressof(current), names, 3)) {
					Actors::SetMode(entry.key, static_cast<ActorMode>(current));
				}
				ImGuiMCP::PopID();
			}

			Heading("Skin wet maps");
			Wrapped("A skin patch can ship wet maps made for its own normal map (for example a cellulite skin). Auto: a character gets them only when wearing that skin. Pick a skin to force its maps on a character with a custom skin, or Shared maps only to keep Drench's own maps.");
			const auto skins = Actors::AllSkins();
			if (skins.empty()) {
				Wrapped("None: every character is on Auto.");
			}
			for (std::size_t i = 0; i < skins.size(); ++i) {
				const auto& entry = skins[i];
				ImGuiMCP::PushID(static_cast<int>(1000 + i));
				if (const auto choice = SkinCombo(Actors::Describe(entry.key).c_str(), entry.choice)) {
					Actors::SetSkinChoice(entry.key, *choice);
				}
				ImGuiMCP::PopID();
			}
			const auto& folders = SkinFolders();
			ImGuiMCP::Text("%s: %zu", T("Skins with wet maps installed"), folders.size());
			for (const auto& folder : folders) {
				ImGuiMCP::BulletText("%s", folder.c_str());
			}
			if (Button("Rescan skins")) {
				SkinFolders(true);
			}
			Wrapped("Stored in Data\\SKSE\\Plugins\\Drench_Actors.ini");
		}

		void __stdcall VisualsPage()
		{
			auto& s = Settings::Get();
			Heading("Wet maps");
			static const char* const textureModes[]{ "On every skin", "Shine only on UBE skins", "Shine only everywhere" };
			const char*              textureNames[]{ T(textureModes[0]), T(textureModes[1]), T(textureModes[2]) };
			int                      textureMode = std::clamp(s.iTextureMode, 0, 2);
			if (ImGuiMCP::Combo(T("Wet maps"), std::addressof(textureMode), textureNames, 3)) {
				s.iTextureMode = textureMode;
				Commit();
			}
			Help("On UBE the wet map takes the place of the skin's own normal map, so pores and fine skin detail give way to the drops. Shine only keeps the skin as it is and only makes it shine.");

			Heading("Specular");
			Slider("Specular minimum", s.fSpecularMin, 0.0f, 30.0f, "%.1f");
			Slider("Specular maximum", s.fSpecularMax, 0.0f, 30.0f, "%.1f");
			Slider("Force specular", s.fSpecularForce, 0.0f, 30.0f, "%.1f", "0 releases the forced value.");
			Slider("Head specular multiplier", s.fHeadSpecularMult, 0.0f, 5.0f, "%.2f");
			Slider("Hand specular multiplier", s.fHandSpecularMult, 0.0f, 5.0f, "%.2f");
			Toggle("Specular: body", s.bSpecularBody);
			Toggle("Specular: hands", s.bSpecularHands);
			Toggle("Specular: feet", s.bSpecularFeet);
			Toggle("Specular: head", s.bSpecularHead);
			Toggle("Specular: ears and other skin head parts", s.bSpecularHeadParts, "Head parts with a skin shader besides the face itself get the shine, not the wet map.");

			Heading("Glossiness");
			Slider("Glossiness minimum", s.fGlossinessMin, 0.0f, 1000.0f, "%.1f");
			Slider("Glossiness maximum", s.fGlossinessMax, 0.0f, 1000.0f, "%.1f");
			Slider("Force glossiness", s.fGlossinessForce, 0.0f, 1000.0f, "%.1f", "0 releases the forced value.");
			Toggle("Glossiness: body", s.bGlossinessBody);
			Toggle("Glossiness: hands", s.bGlossinessHands);
			Toggle("Glossiness: feet", s.bGlossinessFeet);
			Toggle("Glossiness: head", s.bGlossinessHead);

			Heading("Wet textures");
			Toggle("Body textures", s.bTextureBody);
			Toggle("Body drops", s.bTextureBodyDrops);
			Toggle("Body sweat", s.bTextureBodySweat);
			Toggle("Body pussy variant", s.bTextureBodyPussy);
			Toggle("Body drops in relief (3BA with Mu Dynamic NormalMap)", s.bTextureBodyRelief, "Mu Dynamic NormalMap rebuilds the 3BA body's normal map from its real shape. Drench hands it a detail map with the drops, so they stand up on the skin. Without Mu Dynamic NormalMap the 3BA body gets the wet shine only.");
			Toggle("Feet drops", s.bTextureFeetDrops);
			Toggle("Feet sweat", s.bTextureFeetSweat);
			Toggle("Hands", s.bTextureHands);
			Toggle("Head", s.bTextureHead);
			Toggle("Male schlong", s.bTextureSchlong);
			Toggle("Futanari schlong", s.bTextureFutaSchlong);
		}

		void __stdcall Hud()
		{
			auto& s = Settings::Get();
			Wrapped("A droplet that fills from the bottom with your wetness. The arrow beside it points up while you get wetter and down while you dry.");
			Heading("Droplet");
			Toggle("Show droplet", s.bWidgetEnabled);
			Toggle("Show while dry", s.bWidgetAlways, "Off: the droplet fades in when you get wet and out when you are dry.");
			Slider("Horizontal position", s.fWidgetX, 0.0f, 1.0f, "%.3f");
			Slider("Vertical position", s.fWidgetY, 0.0f, 1.0f, "%.3f");
			Slider("Size", s.fWidgetSize, 16.0f, 160.0f, "%.0f px", "Height at 1080p; it scales with the screen.");
			Slider("Opacity", s.fWidgetOpacity, 0.1f, 1.0f, "%.2f");
		}

		void __stdcall Diagnostics()
		{
			const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
			const auto  warn = ImGuiMCP::ImVec4(1.0f, 0.55f, 0.35f, 1.0f);
			const auto  good = ImGuiMCP::ImVec4(0.55f, 0.9f, 0.55f, 1.0f);

			Heading("Installation");
			ImGuiMCP::Text("Drench %s", plugin->GetVersion().string().c_str());
			ImGuiMCP::Text("%s: %s", T("Settings"), Status::settingsSource.c_str());
			if (Status::conflict.empty()) {
				ImGuiMCP::TextColored(good, "%s", T("No conflicting wetness mod loaded."));
			} else {
				ImGuiMCP::TextColored(warn, "%s", Status::conflict.c_str());
			}
			ImGuiMCP::Text("%s: %s", T("RaceMenu"), Status::raceMenu ? T("found (only used to remove old WetFunction overrides)") : T("not found (not needed)"));
			ImGuiMCP::Text("%s: %s", T("Mu Dynamic NormalMap"), Mdnm::Available() ? T("found (3BA drops in relief)") : T("not found (3BA gets the wet shine only)"));
			ImGuiMCP::Text("%s: %s", T("Menu language"), Strings::Language().empty() ? "?" : Strings::Language().c_str());

			Heading("Wet maps");
			const auto summary = Visuals::Summarize();
			if (summary.sets.empty()) {
				Wrapped("Checked when the first actor gets wet.");
			}
			for (const auto& line : summary.sets) {
				ImGuiMCP::BulletText("%s", line.c_str());
			}
			ImGuiMCP::Text("%s: %zu, %s: %zu, %s: %zu", T("Painted actors"), summary.actors, T("painted parts"), summary.geometries, T("maps held loaded"), summary.textures);

			Heading("Managed actors");
			const auto tracked = Manager::Get().Snapshot();
			if (tracked.empty()) {
				Wrapped("None.");
			}
			if (!tracked.empty() && ImGuiMCP::BeginTable("##tracked", 4)) {
				ImGuiMCP::TableSetupColumn(T("Name"));
				ImGuiMCP::TableSetupColumn(T("Wetness"));
				ImGuiMCP::TableSetupColumn(T("State"));
				ImGuiMCP::TableSetupColumn(T("Started by"));
				ImGuiMCP::TableHeadersRow();
				for (const auto& actor : tracked) {
					ImGuiMCP::TableNextRow();
					ImGuiMCP::TableNextColumn();
					ImGuiMCP::Text("%s%s%s", actor.name.c_str(), actor.loaded ? "" : " ", actor.loaded ? "" : T("(not loaded)"));
					ImGuiMCP::TableNextColumn();
					ImGuiMCP::Text("%.2f / %.0f", actor.wetness, Settings::Get().fWetnessCap);
					ImGuiMCP::TableNextColumn();
					ImGuiMCP::Text("%s", T(std::string(PhaseName(actor.phase)).c_str()));
					ImGuiMCP::TableNextColumn();
					ImGuiMCP::Text("%s", actor.manual ? T("player or a mod") : T("automatic"));
				}
				ImGuiMCP::EndTable();
			}

			Heading("Report");
			Wrapped("Writes everything Drench knows about an actor - wetness, every skin part with its shader values and textures, what Drench wrote there - to the console and to Drench.log. Attach that log when you report a problem.");
			if (Button("Report the player")) {
				Visuals::Out(Manager::Get().Status(RE::PlayerCharacter::GetSingleton()));
				Visuals::Dump(RE::PlayerCharacter::GetSingleton(), "report");
			}
			if (auto* picked = PickedActor(); picked && !picked->IsPlayerRef()) {
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(std::format("{}: {}", T("Report"), picked->GetName()).c_str())) {
					Visuals::Out(Manager::Get().Status(picked));
					Visuals::Dump(picked, "report");
				}
			}
		}
	}

	bool Register()
	{
		if (!GetMenuFrameworkModule()) {
			logger::warn("SKSE Menu Framework is not loaded; configuration UI is unavailable");
			return false;
		}
		SKSEMenuFramework::SetSection("Drench");
		SKSEMenuFramework::AddSectionItem(T("General"), General);
		SKSEMenuFramework::AddSectionItem(T("Wetness"), Wetness);
		SKSEMenuFramework::AddSectionItem(T("Sources"), Sources);
		SKSEMenuFramework::AddSectionItem(T("Integrations"), Integrations);
		SKSEMenuFramework::AddSectionItem(T("Automatic actors"), Automatic);
		SKSEMenuFramework::AddSectionItem(T("Characters"), Characters);
		SKSEMenuFramework::AddSectionItem(T("Visuals"), VisualsPage);
		SKSEMenuFramework::AddSectionItem(T("HUD"), Hud);
		SKSEMenuFramework::AddSectionItem(T("Diagnostics"), Diagnostics);
		logger::info("Registered SKSE Menu Framework configuration (framework {})", SKSEMenuFramework::GetMenuFrameworkVersion());
		return true;
	}
}
