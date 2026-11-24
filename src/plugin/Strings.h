#pragma once

// Menu text in the game's language. The English text is the key: Data/SKSE/Plugins/Drench/Strings_<LANGUAGE>.ini
// holds lines "English text = translation" (LANGUAGE from the game's sLanguage, e.g. RUSSIAN); anything not
// translated shows in English. Strings_TEMPLATE.ini lists every text there is.
namespace Drench::Strings
{
	void        Load();                      // once the game's INI is read; again changes nothing
	const char* T(const char* a_english);    // the translation, or a_english itself; the pointer stays valid
	std::string Language();                  // "RUSSIAN", or "" when the game did not say
	std::size_t Count();                     // translations loaded
}
