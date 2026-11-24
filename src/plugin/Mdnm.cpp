#include "Mdnm.h"

#include "core/Core.h"

namespace
{
	// ABI-compatible declaration of the interface Mu Dynamic NormalMap exports (MIT licence, Acro748,
	// github.com/Acro748/MuDynamicNormalMap, include/Interface/Interface.h, interface version 0).
	class Interface
	{
	public:
		virtual ~Interface() = default;
		virtual std::uint32_t GetVersion() = 0;
	};

	class IDynamicNormalMap : public Interface
	{
	public:
		virtual void QUpdateNormalmap(RE::Actor* a_actor, std::uint32_t a_updateBipedSlot) = 0;
		virtual void SetDetailStrength(RE::Actor* a_actor, float a_strength) = 0;
		// type 0 detail, 1 overlay, 2 mask; returns 1 success, 0 invalid parameters, -1 no such file
		virtual int SetNormalMap(RE::Actor* a_actor, const char* a_filePath, int a_type) = 0;
	};

	struct ExchangeMessage
	{
		static constexpr std::uint32_t kExchangeInterface = 0x4D444E4D;  // 'MDNM'
		Interface*                     iface = nullptr;
	};

	constexpr int           kDetail = 0;
	constexpr std::uint32_t kBodyAndFeet = (1u << 2) | (1u << 7);  // BIPED_OBJECT body 32, feet 37

	struct Worn
	{
		std::string detail;  // the Drench detail map handed to MDNM
		std::string dry;     // what goes back when the actor dries
	};

	IDynamicNormalMap*                     g_api = nullptr;
	std::mutex                             g_lock;
	std::unordered_map<RE::FormID, Worn>   g_worn;

	bool Exists(const std::string& a_path)
	{
		RE::BSResourceNiBinaryStream stream(a_path);
		return stream.good();
	}

	// MDNM's own rule for a skin's detail map: <name>_msn.dds -> <name>_n.dds in the same folder. Its proxy folders
	// are not searched here; an actor whose detail map lives only there gets the flat one until the game restarts.
	std::string DryDetail(std::string a_bodyNormal)
	{
		std::ranges::transform(a_bodyNormal, a_bodyNormal.begin(), [](char a_char) { return static_cast<char>(std::tolower(static_cast<unsigned char>(a_char))); });
		if (!a_bodyNormal.empty() && !a_bodyNormal.starts_with("textures\\")) {
			a_bodyNormal = "textures\\" + a_bodyNormal;
		}
		constexpr std::string_view msn = "_msn.dds";
		if (a_bodyNormal.ends_with(msn)) {
			auto own = a_bodyNormal.substr(0, a_bodyNormal.size() - msn.size()) + "_n.dds";
			if (Exists(own)) {
				return own;
			}
		}
		return Drench::FlatDetailTexture();
	}

	bool Hand(RE::Actor* a_actor, const std::string& a_path)
	{
		const int result = g_api->SetNormalMap(a_actor, a_path.c_str(), kDetail);
		if (result != 1) {
			logger::warn("Mu Dynamic NormalMap refused detail map '{}' for {} [{:08X}] ({})", a_path, a_actor->GetName(), a_actor->GetFormID(),
				result < 0 ? "no such file" : "invalid parameters");
			return false;
		}
		g_api->QUpdateNormalmap(a_actor, kBodyAndFeet);
		return true;
	}
}

namespace Drench::Mdnm
{
	void Load()
	{
		if (!::GetModuleHandleW(L"MuDynamicNormalMap.dll")) {
			logger::info("Mu Dynamic NormalMap not loaded: 3BA bodies get the wet shine maps only");
			return;
		}
		ExchangeMessage message;
		SKSE::GetMessagingInterface()->Dispatch(ExchangeMessage::kExchangeInterface, std::addressof(message), sizeof(message), "MuDynamicNormalMap");
		if (!message.iface) {
			logger::warn("Mu Dynamic NormalMap is loaded but gave no interface: 3BA bodies get the wet shine maps only");
			return;
		}
		const auto version = message.iface->GetVersion();
		if (version != 0) {
			logger::warn("Mu Dynamic NormalMap interface version {} is unknown (0 expected): not used", version);
			return;
		}
		g_api = static_cast<IDynamicNormalMap*>(message.iface);
		logger::info("Mu Dynamic NormalMap interface found: wet 3BA bodies get volumetric drops through its detail map");
	}

	bool Available() { return g_api != nullptr; }

	void Want(RE::Actor* a_actor, const std::string& a_detail, const std::string& a_bodyNormal)
	{
		if (!g_api || !a_actor) {
			return;
		}
		std::scoped_lock lock(g_lock);
		const auto       id = a_actor->GetFormID();
		const auto       it = g_worn.find(id);
		if (a_detail.empty()) {
			if (it != g_worn.end()) {
				Hand(a_actor, it->second.dry);
				logger::info("{} [{:08X}]: detail map back to '{}'", a_actor->GetName(), id, it->second.dry);
				g_worn.erase(it);
			}
			return;
		}
		if (it != g_worn.end() && it->second.detail == a_detail) {
			return;
		}
		const auto dry = it != g_worn.end() ? it->second.dry : DryDetail(a_bodyNormal);
		if (Hand(a_actor, a_detail)) {
			g_worn[id] = { a_detail, dry };
			logger::info("{} [{:08X}]: wet detail map '{}' handed to Mu Dynamic NormalMap (dry: '{}')", a_actor->GetName(), id, a_detail, dry);
		}
	}

	void RestoreAll()
	{
		if (!g_api) {
			return;
		}
		std::scoped_lock lock(g_lock);
		for (const auto& [id, worn] : g_worn) {
			// MDNM keeps the path per form id across loads, so it is set back even for an actor that is not loaded
			if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
				Hand(actor, worn.dry);
			}
		}
		if (!g_worn.empty()) {
			logger::info("Detail maps back to dry on {} actors", g_worn.size());
		}
		g_worn.clear();
	}

	std::string Describe(RE::FormID a_actor)
	{
		std::scoped_lock lock(g_lock);
		const auto       it = g_worn.find(a_actor);
		return it != g_worn.end() ? it->second.detail : std::string();
	}
}
