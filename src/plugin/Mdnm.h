#pragma once

// Mu Dynamic NormalMap (MDNM) rebuilds the normal map of model-space skin at runtime from the body's real shape and
// puts it into the material itself; a foreign texture in that slot makes it rebuild again. So on a 3BA body Drench
// never writes the normal map: it hands MDNM a tangent detail map with the drops (SetNormalMap, type detail) and asks
// for a body + feet update. Without MDNM nothing here does anything and 3BA keeps the wet shine maps alone.
namespace Drench::Mdnm
{
	// Asks MDNM for its interface (SKSE message exchange). Call once, at DataLoaded.
	void Load();
	bool Available();

	// The detail map an actor should wear: a Drench wet detail map, or empty for "dry" - then the skin's own detail
	// map (<body normal>_n.dds next to its _msn, as MDNM itself finds it) or a flat one goes back. a_bodyNormal is the
	// body's normal map path from its texture set, remembered for the way back. Only calls MDNM when this changes.
	void Want(RE::Actor* a_actor, const std::string& a_detail, const std::string& a_bodyNormal);

	// Every actor that wears a Drench detail map gets its dry one back (load, revert).
	void RestoreAll();

	// "wet_110_detail_n.dds" / "" for the report.
	std::string Describe(RE::FormID a_actor);
}
