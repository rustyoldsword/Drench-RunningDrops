#include "Visuals.h"

#include "Actors.h"
#include "Mdnm.h"
#include "Settings.h"
#include "Skee.h"

namespace Drench::Visuals
{
	namespace
	{
		// biped slots of the skin parts, by core Part: body 32, hands 33, feet 37, head (none), schlong 52
		constexpr std::array<std::uint32_t, kPartCount> kMasks{ 0x04, 0x08, 0x80, 0, 0x400000 };

		// What this plugin wrote on one geometry, and what was there before.
		struct Paint
		{
			float                              specular{ -1.0f };  // written value, <= 0 none
			float                              glossiness{ -1.0f };
			float                              drySpecular{ 0.0f };
			float                              dryGlossiness{ 0.0f };
			std::string                        texture;  // written wet map, "" none
			std::uint8_t                       slot{ 0 };
			RE::NiPointer<RE::NiSourceTexture> dryTexture;  // the texture object the wet map replaced

			bool Any() const { return specular > 0.0f || glossiness > 0.0f || !texture.empty(); }
		};

		// keyed by "view|part|geometry name": the geometry object itself is replaced on every 3D rebuild
		using Paints = std::unordered_map<std::string, Paint>;

		struct Target
		{
			Part            part;
			bool            shineOnly;  // a skin head part besides the face: the shine, a wet map only for Goam ears
			RE::BSGeometry* geometry;
			std::string     key;
		};

		std::recursive_mutex                   g_lock;
		std::unordered_map<RE::FormID, Paints> g_states;
		std::unordered_set<RE::FormID>         g_legacyChecked;
		std::unordered_map<std::string, bool>  g_files;

		// Wet textures stay referenced once loaded: the engine drops a texture as soon as no material uses it, so
		// every swap of a 4K map would load it again on the main thread.
		std::unordered_map<std::string, RE::NiPointer<RE::NiSourceTexture>> g_textures;
		std::map<std::string, std::string>                                  g_checkedSets;  // skin type -> what was found

		// Loads on demand, on the calling thread. Asked without demand the engine hands back its default texture
		// ("BSShader_DefNormalMap") while the real one is still queued; put into a specular slot that makes the whole
		// skin shine like metal (seen in game 22.11), so such an answer is never used or kept.
		RE::NiPointer<RE::NiSourceTexture> Load(const std::string& a_path)
		{
			if (const auto it = g_textures.find(a_path); it != g_textures.end()) {
				return it->second;
			}
			RE::NiPointer<RE::NiTexture> texture;
			RE::BSShaderManager::GetTexture(a_path.c_str(), true, texture, false);
			if (!texture || std::string_view(texture->name.c_str()).starts_with("BSShader_Def")) {
				logger::warn("Wet texture {} did not load", a_path);
				return nullptr;
			}
			return g_textures.emplace(a_path, static_cast<RE::NiSourceTexture*>(texture.get())).first->second;
		}

		// Whether a texture object is one of our wet maps. By identity, not by name: every map we put on a skin
		// went through Load() and stays in the cache.
		bool IsOurs(const RE::NiSourceTexture* a_texture)
		{
			return a_texture && std::ranges::any_of(g_textures, [&](const auto& a_entry) { return a_entry.second.get() == a_texture; });
		}

		// loose files and archives alike
		bool FileExists(const std::string& a_path, bool a_warn = true)
		{
			if (const auto it = g_files.find(a_path); it != g_files.end()) {
				return it->second;
			}
			bool exists = false;
			{
				RE::BSResourceNiBinaryStream stream(a_path);
				exists = stream.good();
			}
			if (!exists) {
				std::error_code ec;
				exists = std::filesystem::exists(std::filesystem::path("Data") / a_path, ec);
			}
			if (!exists && a_warn) {
				logger::warn("Missing wet texture {}", a_path);
			}
			return g_files.emplace(a_path, exists).first->second;
		}

		bool IsFemale(RE::Actor* a_actor)
		{
			const auto* npc = a_actor->GetActorBase();
			return npc && npc->GetSex() == RE::SEX::kFemale;
		}

		std::string RaceEditorID(RE::Actor* a_actor)
		{
			const auto* race = a_actor->GetRace();
			const char* id = race ? race->GetFormEditorID() : nullptr;
			return id ? id : "";
		}

		RE::BSLightingShaderProperty* Shader(RE::BSGeometry* a_geometry)
		{
			auto* shader = netimmerse_cast<RE::BSLightingShaderProperty*>(a_geometry->GetGeometryRuntimeData().shaderProperty.get());
			return shader && shader->material ? shader : nullptr;
		}

		// A COTR face, by the geometry the engine draws (core IsCotrHead)
		bool IsCotr(RE::BSGeometry* a_geometry)
		{
			auto*      shape = a_geometry->AsTriShape();
			auto*      shader = Shader(a_geometry);
			const bool modelSpace = shader && shader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals);
			return shape && IsCotrHead(shape->GetTrishapeRuntimeData().vertexCount, modelSpace);
		}

		RE::BSLightingShaderMaterialBase* Material(RE::BSGeometry* a_geometry)
		{
			auto* shader = Shader(a_geometry);
			return shader ? static_cast<RE::BSLightingShaderMaterialBase*>(shader->material) : nullptr;
		}

		// the path the mesh was built with; stays the dry one while a wet map sits in the material
		std::string SetPath(RE::BSGeometry* a_geometry, std::uint32_t a_index)
		{
			auto*       material = Material(a_geometry);
			auto*       set = material ? material->textureSet.get() : nullptr;
			const char* path = set ? set->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(a_index)) : nullptr;
			return path ? path : "";
		}

		bool HasFeature(RE::BSGeometry* a_geometry, RE::BSShaderMaterial::Feature a_feature)
		{
			auto* material = Material(a_geometry);
			return material && material->GetFeature() == a_feature;
		}

		// skin geometry of one biped slot under one root (third or first person)
		std::vector<RE::BSGeometry*> SlotGeometries(RE::Actor* a_actor, RE::NiAVObject* a_root, std::uint32_t a_mask)
		{
			std::vector<RE::BSGeometry*> result;
			auto*                        armor = a_actor->GetSkin(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(a_mask));
			if (!a_root || !armor) {
				return result;
			}
			for (auto* addon : armor->armorAddons) {
				if (!addon || (addon->bipedModelData.bipedObjectSlots.underlying() & a_mask) == 0) {
					continue;
				}
				char name[260]{};
				addon->GetNodeName(name, a_actor, armor, -1.0f);
				auto* node = a_root->GetObjectByName(RE::BSFixedString(name));
				if (!node) {
					continue;
				}
				RE::BSVisit::TraverseScenegraphGeometries(node, [&](RE::BSGeometry* a_geometry) {
					if (HasFeature(a_geometry, RE::BSShaderMaterial::Feature::kFaceGenRGBTint) && std::ranges::find(result, a_geometry) == result.end()) {
						result.push_back(a_geometry);
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			}
			return result;
		}

		// the face, as the engine itself finds it
		std::vector<RE::BSGeometry*> FaceGeometries(RE::Actor* a_actor)
		{
			std::vector<RE::BSGeometry*> result;
			auto*                        object = a_actor->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kFace);
			if (!object) {
				return result;
			}
			if (auto* geometry = object->AsGeometry()) {
				if (Material(geometry)) {
					result.push_back(geometry);
				}
			} else {
				RE::BSVisit::TraverseScenegraphGeometries(object, [&](RE::BSGeometry* a_geometry) {
					if (Material(a_geometry)) {
						result.push_back(a_geometry);
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			}
			return result;
		}

		// Ears and other head parts that are skin besides the face. A skin shader alone is not enough: eye corners
		// have one too (seen in game 22.11). A part counts when it wears the face's own diffuse texture, or when it
		// has its own specular map - skin does, eye corners and lashes do not. Separate ear mods (Goam Elven Ears)
		// bring their own diffuse and specular maps, so the first rule alone missed them (seen in game 22.11).
		// a_rejected, when given, collects the skin-shaded parts that did not count, for the report.
		std::vector<RE::BSGeometry*> SkinHeadParts(RE::Actor* a_actor, const std::vector<RE::BSGeometry*>& a_face, std::vector<RE::BSGeometry*>* a_rejected = nullptr)
		{
			std::vector<RE::BSGeometry*> result;
			auto*                        node = a_actor->GetFaceNodeSkinned();
			if (!node || a_face.empty()) {
				return result;
			}
			const auto same = [](const std::string& a_left, const std::string& a_right) {
				return !a_left.empty() && std::ranges::equal(a_left, a_right, [](char a, char b) {
					return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
				});
			};
			const auto faceDiffuse = SetPath(a_face.front(), 0);
			RE::BSVisit::TraverseScenegraphGeometries(node, [&](RE::BSGeometry* a_geometry) {
				const bool skin = HasFeature(a_geometry, RE::BSShaderMaterial::Feature::kFaceGenRGBTint) || HasFeature(a_geometry, RE::BSShaderMaterial::Feature::kFaceGen);
				if (!skin || std::ranges::find(a_face, a_geometry) != a_face.end()) {
					return RE::BSVisit::BSVisitControl::kContinue;
				}
				if (same(SetPath(a_geometry, 0), faceDiffuse) || !SetPath(a_geometry, 7).empty()) {
					result.push_back(a_geometry);
				} else if (a_rejected) {
					a_rejected->push_back(a_geometry);
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			return result;
		}

		std::vector<Target> Targets(RE::Actor* a_actor, const Settings& a_settings)
		{
			std::vector<Target>                          targets;
			std::unordered_map<std::string, std::size_t> seen;
			const auto add = [&](std::string_view a_view, Part a_part, bool a_shineOnly, RE::BSGeometry* a_geometry) {
				auto key = std::format("{}|{}{}|{}", a_view, PartName(a_part), a_shineOnly ? "+" : "", a_geometry->name.c_str());
				if (const auto count = seen[key]++; count > 0) {
					key += std::format("#{}", count);
				}
				targets.push_back({ a_part, a_shineOnly, a_geometry, std::move(key) });
			};

			auto* third = a_actor->Get3D(false);
			auto* first = a_actor->IsPlayerRef() && a_settings.bFirstPerson ? a_actor->Get3D(true) : nullptr;
			for (const auto& [view, root] : { std::pair{ "3p"sv, third }, std::pair{ "1p"sv, first == third ? nullptr : first } }) {
				if (!root) {
					continue;
				}
				for (const auto part : { Part::Body, Part::Hands, Part::Feet, Part::Schlong }) {
					for (auto* geometry : SlotGeometries(a_actor, root, kMasks[static_cast<std::size_t>(part)])) {
						add(view, part, false, geometry);
					}
				}
			}
			const auto face = FaceGeometries(a_actor);
			for (auto* geometry : face) {
				add("3p"sv, Part::Head, false, geometry);
			}
			if (a_settings.bSpecularHeadParts) {
				for (auto* geometry : SkinHeadParts(a_actor, face)) {
					add("3p"sv, Part::Head, true, geometry);
				}
			}
			return targets;
		}

		// Read from the first skin the actor shows: the body, or under clothes the hands, the feet, the face.
		// (Taking the body alone left a dressed actor's hands and face unpainted: "no skin".)
		SkinMode ModeOf(RE::Actor* a_actor)
		{
			auto*           root = a_actor->Get3D(false);
			RE::BSGeometry* skin = nullptr;
			for (const auto part : { Part::Body, Part::Hands, Part::Feet }) {
				if (const auto found = SlotGeometries(a_actor, root, kMasks[static_cast<std::size_t>(part)]); !found.empty()) {
					skin = found.front();
					break;
				}
			}
			if (!skin) {
				if (const auto face = FaceGeometries(a_actor); !face.empty()) {
					skin = face.front();
				}
			}
			if (!skin) {
				return SkinMode::None;
			}
			auto*      shader = Shader(skin);
			const bool modelSpace = shader && shader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals);
			return ModeFor(true, modelSpace, RaceEditorID(a_actor), SetPath(skin, 1));
		}

		// The body's normal map from its texture set ("" when the body is covered): where MDNM finds the skin's own
		// detail map, which goes back when the actor dries.
		std::string BodyNormalPath(RE::Actor* a_actor)
		{
			const auto found = SlotGeometries(a_actor, a_actor->Get3D(false), kMasks[static_cast<std::size_t>(Part::Body)]);
			return found.empty() ? std::string() : SetPath(found.front(), 1);
		}

		// The material's own texture pointer for a texture set index (1 normal, 7 specular), or null.
		RE::NiPointer<RE::NiSourceTexture>* Slot(RE::BSLightingShaderMaterialBase* a_material, std::uint8_t a_index)
		{
			switch (a_index) {
			case 1:
				return std::addressof(a_material->normalTexture);
			case 7:
				return std::addressof(a_material->specularBackLightingTexture);
			default:
				return nullptr;
			}
		}

		std::string LiveName(RE::BSLightingShaderMaterialBase* a_material, std::uint8_t a_index)
		{
			auto* slot = Slot(a_material, a_index);
			return slot && *slot ? std::string((*slot)->name.c_str()) : std::string();
		}

		// Our map out of the slot, the remembered texture back in. A rebuilt geometry no longer carries our map
		// and is left as it is.
		void RestoreTexture(RE::BSGeometry* a_geometry, RE::BSLightingShaderMaterialBase* a_material, Paint& a_paint)
		{
			if (auto* slot = Slot(a_material, a_paint.slot); slot && IsOurs(slot->get())) {
				auto dry = a_paint.dryTexture;
				if (!dry) {
					// nothing remembered (it should be): the mesh's own path still names the dry texture
					if (const auto path = SetPath(a_geometry, a_paint.slot); !path.empty() && !IsOurTexture(path)) {
						RE::NiPointer<RE::NiTexture> texture;
						RE::BSShaderManager::GetTexture(path.c_str(), true, texture, false);
						dry.reset(static_cast<RE::NiSourceTexture*>(texture.get()));
					}
				}
				*slot = dry;
			}
			a_paint.texture.clear();
			a_paint.dryTexture.reset();
		}

		// The texture pointer is replaced in place. Rebuilding the material from a new texture set drops the head's
		// runtime face tint texture (the face turns darker than the body), so the texture set is never touched.
		void EnsureTexture(RE::BSGeometry* a_geometry, RE::BSLightingShaderMaterialBase* a_material, const Want& a_want, Paint& a_paint)
		{
			if (!a_paint.texture.empty() && (a_want.texture.empty() || a_want.slot != a_paint.slot)) {
				RestoreTexture(a_geometry, a_material, a_paint);
			}
			if (a_want.texture.empty()) {
				return;
			}
			auto*      slot = Slot(a_material, a_want.slot);
			const auto wet = slot ? Load(a_want.texture) : nullptr;
			if (!wet) {
				return;
			}
			if (slot->get() != wet.get()) {
				if (!IsOurs(slot->get())) {
					a_paint.dryTexture = *slot;  // the slot holds the skin's own texture: remember that object
				}
				*slot = wet;
			}
			a_paint.texture = a_want.texture;
			a_paint.slot = a_want.slot;
		}

		// a_live is the material's value, a_have what was written last, a_dry what it replaced
		void EnsureFloat(float a_want, float& a_live, float& a_have, float& a_dry)
		{
			const bool ours = a_have > 0.0f && std::abs(a_live - a_have) < 0.005f;
			if (a_want > 0.0f) {
				if (!ours) {
					a_dry = a_live;  // a fresh geometry, or something else set the value: that is the dry one now
				}
				a_live = a_want;
				a_have = a_want;
			} else if (a_have > 0.0f) {
				if (ours) {
					a_live = a_dry;
				}
				a_have = -1.0f;
			}
		}

		void Ensure(const Target& a_target, Want a_want, Paint& a_paint, std::string_view a_skinChoice)
		{
			auto* material = Material(a_target.geometry);
			if (!material) {
				return;
			}
			if (a_target.shineOnly) {
				// a head part with its own maps: Goam ears get the ear map made for the head map planned here
				// (their own UV, model-space normals, so slot 7); anything else keeps its maps and gets the shine
				a_want.texture = EarTexture(a_want.texture, SetPath(a_target.geometry, 7));
				a_want.slot = 7;
				if (!a_want.texture.empty() && !FileExists(a_want.texture)) {
					a_want.texture.clear();
				}
			} else if (a_target.part == Part::Head && !a_want.texture.empty() && IsCotr(a_target.geometry)) {
				// a COTR face: its own UV layout, model-space even on a UBE body - the COTR map of the planned state
				// in slot 7; without one it keeps its own maps and gets the shine (a map of another UV misplaces drops)
				auto cotr = CotrHeadTextures(a_want.texture);
				if (!cotr.texture.empty() && !FileExists(cotr.texture, cotr.fallback.empty())) {
					cotr.texture = !cotr.fallback.empty() && FileExists(cotr.fallback, true) ? cotr.fallback : std::string();
				}
				a_want.texture = std::move(cotr.texture);
				a_want.slot = 7;
			} else if (!a_want.texture.empty()) {
				// a skin patch may ship wet maps made for its own normal map (core SkinWetTexture): only characters
				// wearing that skin get them, everyone else keeps the shared map
				if (auto own = ChosenSkinWetTexture(a_want.texture, SetPath(a_target.geometry, 1), a_skinChoice); !own.empty() && FileExists(own, false)) {
					static std::unordered_set<std::string> logged;
					if (logged.insert(own).second) {
						logger::info("Skin wet map {} used for skin {}", own, SetPath(a_target.geometry, 1));
					}
					a_want.texture = std::move(own);
				}
			}
			EnsureTexture(a_target.geometry, material, a_want, a_paint);
			EnsureFloat(a_want.specular, material->specularColorScale, a_paint.specular, a_paint.drySpecular);
			EnsureFloat(a_want.glossiness, material->specularPower, a_paint.glossiness, a_paint.dryGlossiness);
		}

		// Brings every skin geometry to the plan; paints of geometries that are gone are dropped.
		void Bring(RE::Actor* a_actor, const Plan& a_plan, const Settings& a_settings, Paints& a_paints)
		{
			Paints     kept;
			const auto skinChoice = Actors::SkinChoice(a_actor);  // per character: auto, shared or a forced skin
			for (const auto& target : Targets(a_actor, a_settings)) {
				Paint paint;
				if (const auto it = a_paints.find(target.key); it != a_paints.end()) {
					paint = std::move(it->second);
				}
				Ensure(target, a_plan[static_cast<std::size_t>(target.part)], paint, skinChoice);
				if (paint.Any()) {
					kept.emplace(target.key, std::move(paint));
				}
			}
			a_paints = std::move(kept);
		}

		// once per skin type and session: say in the log which wet maps are installed
		void CheckSet(bool a_female, SkinMode a_mode)
		{
			const auto key = std::format("{}{}", a_female ? 'f' : 'm', static_cast<int>(a_mode));
			if (g_checkedSets.contains(key)) {
				return;
			}
			const auto set = WetTextureSet(a_female, a_mode);
			const auto found = std::ranges::count_if(set, [](const std::string& a_path) { return FileExists(a_path); });
			// head and hand maps per body state are optional (the men's set has none)
			std::size_t states = 0;
			for (const auto base : { "wethead"sv, "wethand"sv }) {
				for (const auto state : { "_10"sv, "_01"sv, "_11"sv }) {
					states += FileExists(WetTexture(a_female, a_mode, std::string(base) + std::string(state)), false) ? 1 : 0;
				}
			}
			const auto line = std::format("{} {} skin: {} of {} maps found, {} of 6 head/hand maps per state", a_female ? "female" : "male",
				a_mode == SkinMode::Normal ? "UBE" : "specular", found, set.size(), states);
			g_checkedSets.emplace(key, line);
			logger::info("Wet maps for {}", line);
		}

		std::string FaceEditorID(RE::Actor* a_actor)
		{
			auto* npc = a_actor->GetActorBase();
			auto* part = npc ? npc->GetHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace) : nullptr;
			return part ? part->formEditorID.c_str() : "";
		}

		std::size_t RemoveLegacy(RE::Actor* a_actor, bool a_everything)
		{
			auto* skee = Skee::Overrides();
			if (!skee) {
				return 0;
			}
			constexpr std::array masks{ 0x04u, 0x08u, 0x80u, 0x400000u };
			std::vector<std::string> nodes{ FaceEditorID(a_actor), "00UBE_FemaleHead", "00UBE_FemaleHeadNPC", "00UBE_MaleHead", "PSQHumanHead", "PSQSuccubusHead" };
			std::erase_if(nodes, [](const std::string& a_node) { return a_node.empty(); });

			std::size_t removed = 0;
			for (const bool female : { false, true }) {
				for (const auto mask : masks) {
					for (const bool fp : { false, true }) {
						for (const std::uint8_t index : { 0, 1, 7 }) {
							Skee::Reader reader;
							if (skee->GetSkinOverride(a_actor, female, fp, mask, Skee::kKeyTexture, index, reader) && reader.text && IsLegacyTexture(*reader.text)) {
								skee->RemoveSkinOverride(a_actor, female, fp, mask, Skee::kKeyTexture, index);
								++removed;
							}
						}
					}
				}
				for (const auto& node : nodes) {
					for (const std::uint8_t index : { 1, 7 }) {
						Skee::Reader reader;
						if (skee->GetNodeOverride(a_actor, female, node.c_str(), Skee::kKeyTexture, index, reader) && reader.text && IsLegacyTexture(*reader.text)) {
							skee->RemoveNodeOverride(a_actor, female, node.c_str(), Skee::kKeyTexture, index);
							++removed;
						}
					}
				}
			}
			// The shine overrides carry no name. They go when a wet map proved WetFunction managed this actor, or on request.
			if (!a_everything && removed == 0) {
				return 0;
			}
			for (const bool female : { false, true }) {
				for (const auto key : { Skee::kKeySpecular, Skee::kKeyGlossiness }) {
					for (const auto mask : masks) {
						for (const bool fp : { false, true }) {
							if (skee->HasSkinOverride(a_actor, female, fp, mask, key, Skee::kNoIndex)) {
								skee->RemoveSkinOverride(a_actor, female, fp, mask, key, Skee::kNoIndex);
								++removed;
							}
						}
					}
					for (const auto& node : nodes) {
						if (skee->HasNodeOverride(a_actor, female, node.c_str(), key, Skee::kNoIndex)) {
							skee->RemoveNodeOverride(a_actor, female, node.c_str(), key, Skee::kNoIndex);
							++removed;
						}
					}
				}
			}
			return removed;
		}
	}

	void Out(const std::string& a_line)
	{
		logger::info("{}", a_line);
		if (auto* console = RE::ConsoleLog::GetSingleton()) {
			console->Print("%s", a_line.c_str());
		}
	}

	bool HasSchlong(RE::Actor* a_actor)
	{
		return a_actor && a_actor->Get3D(false) && !SlotGeometries(a_actor, a_actor->Get3D(false), kMasks[static_cast<std::size_t>(Part::Schlong)]).empty();
	}

	// Which UV layout the schlong wears, told by its own texture names: Schlongs of Skyrim calls them Schlong_d /
	// Schlong_s / Schlong_n whatever the race or texture pack; LDD's racial TNG meshes LDD<Race>, LDD<Race>_s ...
	// (all twelve share one layout).
	Layout SchlongLayout(RE::Actor* a_actor)
	{
		if (!a_actor || !a_actor->Get3D(false)) {
			return Layout::Other;
		}
		const auto name = [](std::string a_path) {
			std::ranges::transform(a_path, a_path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const auto slash = a_path.find_last_of("\\/");
			return a_path.substr(slash == std::string::npos ? 0 : slash + 1);
		};
		for (auto* geometry : SlotGeometries(a_actor, a_actor->Get3D(false), kMasks[static_cast<std::size_t>(Part::Schlong)])) {
			for (const std::uint32_t slot : { 0u, 7u }) {
				const auto file = name(SetPath(geometry, slot));
				if (file.starts_with("schlong")) {
					return Layout::Sos;
				}
				if (file.starts_with("ldd")) {
					return Layout::LddTng;
				}
			}
		}
		return Layout::Other;
	}

	void Apply(RE::Actor* a_actor, const Look& a_look, const Settings& a_settings)
	{
		if (!a_actor || !a_actor->Get3D(false)) {
			return;
		}
		std::scoped_lock lock(g_lock);
		const auto       id = a_actor->GetFormID();
		if (!a_look.active) {
			Clear(a_actor);
			return;
		}
		// once per actor and session: a save made with WetFunction keeps its wet look in RaceMenu's overrides
		if (g_legacyChecked.insert(id).second) {
			if (const auto removed = RemoveLegacy(a_actor, false)) {
				logger::info("{} [{:08X}]: removed {} WetFunction overrides left in the save, rebuilding the 3D", a_actor->GetName(), id, removed);
				a_actor->Update3DModel();
				return;  // the rebuilt skin is painted on the next update
			}
		}

		const auto mode = ModeOf(a_actor);
		const Skin skin{ .mode = mode, .female = IsFemale(a_actor), .schlong = HasSchlong(a_actor),
			.textures = WantsTextures(mode, Actors::Mode(a_actor), a_settings.iTextureMode) };
		if (skin.mode == SkinMode::None) {
			return;
		}
		if (skin.mode == SkinMode::Specular || skin.mode == SkinMode::Normal) {
			CheckSet(skin.female, skin.mode);
		}
		auto plan = PlanLook(a_look, skin, a_settings);
		// A wet schlong map is drawn for one UV layout. The SOS one on a TNG schlong put the drops in the wrong places
		// and the part showed matte (seen in game 22.11), so the map follows the layout the schlong wears: Schlongs of
		// Skyrim, or TNG with the LDD racial meshes (_LDD maps); any other schlong gets the shine on its own maps.
		if (auto& schlong = plan[static_cast<std::size_t>(Part::Schlong)]; !schlong.texture.empty()) {
			const auto layout = SchlongLayout(a_actor);
			if (layout == Layout::Sos) {
				// as planned
			} else if (layout == Layout::LddTng) {
				// TNG has model-space normals whatever the body: the map goes to the specular slot
				schlong.texture = WetTexture(IsFemale(a_actor), SkinMode::Specular, "wetschlong_LDD");
				schlong.slot = TextureSlot(SkinMode::Specular);
			} else {
				schlong.texture.clear();
			}
			schlong.fallback.clear();
		}
		for (auto& want : plan) {
			// a per-state map is optional (the men's set has none): fall back to the part's single map
			if (!want.texture.empty() && !FileExists(want.texture, want.fallback.empty())) {
				want.texture = !want.fallback.empty() && FileExists(want.fallback, true) ? want.fallback : std::string();
			}
		}
		// 3BA with Mu Dynamic NormalMap: the drops as relief, through MDNM's detail map (it owns the normal map)
		if (Mdnm::Available()) {
			const auto& body = plan[static_cast<std::size_t>(Part::Body)].texture;
			const bool  relief = a_settings.bTextureBodyRelief && skin.female && skin.mode == SkinMode::Specular;
			auto        detail = relief ? DetailTexture(body) : std::string();
			if (!detail.empty() && !FileExists(detail)) {
				detail.clear();
			}
			Mdnm::Want(a_actor, detail, BodyNormalPath(a_actor));
		}
		Bring(a_actor, plan, a_settings, g_states[id]);
	}

	Summary Summarize()
	{
		std::scoped_lock lock(g_lock);
		Summary          summary;
		for (const auto& [id, paints] : g_states) {
			summary.actors += paints.empty() ? 0 : 1;
			summary.geometries += paints.size();
		}
		summary.textures = g_textures.size();
		for (const auto& [key, line] : g_checkedSets) {
			summary.sets.push_back(line);
		}
		return summary;
	}

	void Clear(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}
		Mdnm::Want(a_actor, {}, {});
		std::scoped_lock lock(g_lock);
		const auto       it = g_states.find(a_actor->GetFormID());
		if (it == g_states.end()) {
			return;
		}
		if (a_actor->Get3D(false)) {
			Bring(a_actor, Plan{}, Settings::Get(), it->second);
		}
		g_states.erase(it);
	}

	void RestoreAll()
	{
		std::scoped_lock        lock(g_lock);
		std::vector<RE::FormID> ids;
		for (const auto& [id, paints] : g_states) {
			ids.push_back(id);
		}
		std::size_t restored = 0;
		for (const auto id : ids) {
			if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id); actor && actor->Get3D(false)) {
				Clear(actor);
				++restored;
			}
		}
		g_states.clear();
		g_files.clear();
		Mdnm::RestoreAll();
		if (!ids.empty()) {
			logger::info("Dry look restored on {} of {} painted actors", restored, ids.size());
		}
	}

	void Forget(RE::FormID a_actor)
	{
		std::scoped_lock lock(g_lock);
		g_states.erase(a_actor);
	}

	std::size_t CleanLegacy(RE::Actor* a_actor, bool a_everything)
	{
		if (!a_actor) {
			return 0;
		}
		std::scoped_lock lock(g_lock);
		g_legacyChecked.insert(a_actor->GetFormID());
		const auto removed = RemoveLegacy(a_actor, a_everything);
		if (removed && a_actor->Is3DLoaded()) {
			a_actor->Update3DModel();
		}
		return removed;
	}

	void Dump(RE::Actor* a_actor, std::string_view a_title)
	{
		if (!a_actor || !a_actor->Get3D(false)) {
			Out("[Drench] report: no actor or no 3D");
			return;
		}
		std::scoped_lock lock(g_lock);
		const auto&      settings = Settings::Get();
		const auto       state = g_states.find(a_actor->GetFormID());
		Out(std::format("[Drench] {} {} [{:08X}] female {} race {} mode {} schlong {} painted geometries {}", a_title, a_actor->GetName(), a_actor->GetFormID(), IsFemale(a_actor),
			RaceEditorID(a_actor), SkinModeName(ModeOf(a_actor)), HasSchlong(a_actor), state != g_states.end() ? state->second.size() : 0));
		for (const auto& target : Targets(a_actor, settings)) {
			auto* material = Material(target.geometry);
			if (!material) {
				continue;
			}
			std::string ours = "untouched";
			if (state != g_states.end()) {
				if (const auto it = state->second.find(target.key); it != state->second.end()) {
					const auto& paint = it->second;
					ours = std::format("ours: spec {:.2f} (dry {:.2f}) gloss {:.1f} (dry {:.1f}) map '{}' slot {}", paint.specular, paint.drySpecular, paint.glossiness, paint.dryGlossiness,
						paint.texture, paint.slot);
				}
			}
			Out(std::format("  {} spec {:.2f} gloss {:.1f} | n '{}' s '{}' | {}", target.key, material->specularColorScale, material->specularPower, LiveName(material, 1),
				LiveName(material, 7), ours));
			Out(std::format("    mesh paths n '{}' s '{}'", SetPath(target.geometry, 1), SetPath(target.geometry, 7)));
			if (target.part == Part::Head && !target.shineOnly) {
				auto* shape = target.geometry->AsTriShape();
				Out(std::format("    face: {} vertices, {}", shape ? shape->GetTrishapeRuntimeData().vertexCount : 0, IsCotr(target.geometry) ? "COTR head (COTR maps)" : "not COTR"));
			}
		}
		if (Mdnm::Available()) {
			const auto detail = Mdnm::Describe(a_actor->GetFormID());
			Out(std::format("  Mu Dynamic NormalMap detail map: {}", detail.empty() ? "the skin's own (dry)" : detail));
		}
		// skin-shaded head parts left alone, and why: neither the face's diffuse nor a specular map of their own
		std::vector<RE::BSGeometry*> rejected;
		SkinHeadParts(a_actor, FaceGeometries(a_actor), std::addressof(rejected));
		for (auto* geometry : rejected) {
			Out(std::format("  head part not treated as skin: '{}' d '{}' s '{}'", geometry->name.c_str(), SetPath(geometry, 0), SetPath(geometry, 7)));
		}
	}
}
