# Drench - Wet Skin Framework 1.1.0 - Running Drops

This is the **Running Drops** edition: Drench with the embedded DripSim, so water and sweat drops run down the
player's skin. It needs RaceMenu. If you do not want the drops, take the main file of the mod page instead.

Characters get wet from rain, snow, water, sweat, arousal and sex, and dry off gradually. Drench writes the wet look
straight into the skin shader of body, hands, feet, head, ears and genitals. It needs no RaceMenu, no MCM and no
Papyrus polling.

Drench is the successor of WetFunction NG. It is a new mod with its own plugin, scripts and maps. WetFunction NG
1.4.2 stays available as it is.

## Requirements

- Skyrim SE / AE with SKSE64 and Address Library for SKSE Plugins. Tested on AE 1.6.1170. The DLL is built
  for SE 1.5.97 too, but nobody has run it there yet.
- SKSE Menu Framework 3. All settings are in its menu.
- Optional: Mu Dynamic NormalMap, for drops in relief on 3BA bodies.
- Optional: ConsoleUtil Extended, only for the `drench` console commands.
- Optional integrations, each found automatically: SexLab P+ / SexLab, OStim Standalone, SexLab Aroused NG /
  OSL Aroused, Devious Devices NG, Bathing in Skyrim, SunHelm or CC Survival Mode.
- **RaceMenu is required in this edition**: the running drops are drawn into one free RaceMenu body overlay slot
  (and the hand, feet and face ones). The wet skin itself does not use RaceMenu.
- Do not install the separate DripSim mod as well: Drench carries it now. With `DripSim.dll` loaded, Drench's
  drops stay off and the menu says so.

## Installation

1. Disable WetFunction NG, Wet Function Redux, OSweat, Dewpoint and their patches. They paint the same skin, so a
   warning appears on screen if one of them is still loaded.
2. Install this archive with your mod manager:
   - **Main mod** is always installed.
   - **WetFunction NG compatibility** is recommended. Mods written for WetFunction NG keep working: SexLab
     Survival, Diary of Mine, Vitals HUD, SexLab P++ and others. It is an empty ESL `WetFunctionNG.esp`, so their
     checks find "WetFunction NG", and a `WetFunctionNG` script that forwards every call to Drench. Install it
     only together with Drench, never next to the real WetFunction NG.
   - **Female body** - pick the one your game uses; the installer recommends the 3BA one that fits your setup:
     - *3BA (CBBE) with Mu Dynamic NormalMap - drops in relief (best)*: drops and streaks stand up on the 3BA body
       in real relief, like on UBE. Needs [Mu Dynamic NormalMap](https://www.nexusmods.com/skyrimspecialedition/mods/154657):
       it rebuilds the body's normal map from its real shape, and Drench hands it the drops as a detail map.
     - *3BA (CBBE) without Mu Dynamic NormalMap - wet shine only*: drops and sweat drawn in the shine, close to
       WetFunction NG.
     - *BHUNP / UNP - wet shine only*: for BHUNP, UUNP and UNP bodies. The wet body and hand maps are redrawn for
       the UNP UV layout and replace the 3BA ones.
   - **UBE bodies** - only if your game has UBE characters. Recommended when `UBE_AllRace.esp` is active. Without
     it a UBE character still gets the wet shine.

   COTR heads (Charmers of the Reach and the NPC replacers built on it) are always installed: many replacers bake
   COTR heads into their FaceGen, so you may have them without knowing it.
3. In game, open SKSE Menu Framework > **Drench**. The defaults suit most people.

You can add Drench to an existing save. On its first start it takes your settings from `WetFunctionNG.ini` if
there is one. It writes its own `Data/SKSE/Plugins/Drench.ini` the first time you change something in the menu.
Wet overrides that WetFunction left in the save are removed from each character once.

## What it does

- **Wetness** rises from rain and snow, wading and swimming (by how deep the character stands), running,
  fighting, spellcasting, arousal and sex. It falls when the character dries off, faster near campfires,
  fireplaces, braziers and forges. The state goes from dry to sweaty to soaked, and the look changes smoothly.
- **Wet maps on every skin part:** body, hands, feet, head, genitals. Each state has its own map: drops,
  sweat, or both. Head and hand maps are matched to the body map at the neck and the wrists, so no seam shows
  there.
- **Bodies:** CBBE 3BA and BHUNP / UNP (specular maps) and UBE (normal maps with specular in alpha), female and
  male (HIMBO). Drops are in relief on UBE, and on female 3BA bodies with Mu Dynamic NormalMap (sweat stays in the
  shine).
- **Heads:** vanilla and High Poly Head, UBE, and COTR heads, which have a UV layout of their own. A COTR face is
  recognised by its mesh, whatever the race, the NPC replacer or the texture names.
- **Ears:** [Goam Elven Ears - Redone](https://www.nexusmods.com/skyrimspecialedition/mods/165502) in all three
  sizes get wet maps of their own, matched to the head they sit on. Other ear and head parts that are skin get the
  wet shine on their own maps.
- **Genitals:** Schlongs of Skyrim, and TNG with the LDD racial meshes, each get a map drawn for their own UV
  layout. Any other schlong gets the wet shine on its own maps.
- **Characters:** each character can be set to *normal*, *shine only* (skin keeps its own maps and gets the wet
  shine, for skins with scales or other details in their normal maps) or *excluded* (never wet). The setting is
  kept in `Data/SKSE/Plugins/Drench_Actors.ini` and applies to every save. Set it on the Characters page, from the
  console or from Papyrus. A global texture mode does the same for everyone: all maps, shine only on UBE, or shine
  only.
- **Running drops on the player** (the embedded DripSim): water and sweat drops on body, hands, feet and face that
  merge, run down along the live pose and dry. Sweat drops follow Drench's sweat, from its start until soaked with
  sweat. Water drops come from rain under open sky, leaving deep water, washing, and scripts that soak the player.
  Three switches in the menu; fine tuning in `SKSE/Plugins/Drench/Drips.ini`, `DripsWater.ini` and
  `DripsSweat.ini` (F10 re-reads them in game).
- **Wet maps for a specific skin:** a skin texture can carry wet maps made for it (for example a cellulite skin), in
  `textures/actors/character/Drench/skins/<path of the skin's normal map>/`. Drench uses them for characters who
  wear that skin and the shared maps for everyone else. The Characters page has a "Skin wet maps" choice per
  character (Auto, shared maps only, or a set picked by hand).
- **Glossiness** goes up to 1000.
- **Who gets wet:** the player, followers and SexLab/OStim scene partners by default. All nearby NPCs can be
  switched on, filtered by sex, beasts and vampires, with a radius and interval.
- **HUD:** an optional droplet that fills with the player's wetness and shows whether it rises or falls.
- **Saves:** wetness is kept in the SKSE co-save. Loading a save, or reverting to the main menu, puts every
  painted character back to dry first, so nothing sticks across saves.
- **Menu text** can be translated: copy `SKSE/Plugins/Drench/Strings_TEMPLATE.ini` to
  `Strings_<LANGUAGE>.ini` (the game's language name, e.g. `Strings_GERMAN.ini`) and fill in the right-hand
  sides.

## Common mistakes

- **Keeping WetFunction NG, Wet Function Redux, OSweat or Dewpoint enabled.** They fight Drench over the same
  skin.
- **Community Shaders "Advanced Skin", or Vanilla Fresnel > "Enable Phong to GGX".** Both replace the skin
  lighting that carries the wet look. Turn them off.
- **Renaming `WetFunctionNG.esp` from the compatibility option.** Mods look for that exact name.
- **Looking for an MCM.** There is none. Use SKSE Menu Framework.

## Console (ConsoleUtil Extended)

`drench status | start | stop | soak | dry | release | report | clean | reload | stopall | shine | exclude |
normal` act on the actor under the crosshair, or on the player. `drench report` lists every skin part with
its live shader values and the map Drench put there. Please attach it to bug reports.

## For mod authors

- Papyrus: script `Drench`. It has `StartEffect`, `StopEffect`, `IsActive`, `GetWetness`, `SetWetness`,
  `GetWetnessCap`, `ForceValues`, `GetStatus`, `SetActorMode` / `GetActorMode` ("normal", "shine",
  "excluded"), `Bathed`, `RefreshVisuals`, `SexLabUpdate` / `SexLabOrgasm` and more. See
  `Scripts/Source/Drench.psc`. Mods written against `WetFunctionNG` keep working through the compatibility
  option.
- SKSE: `Drench.dll` exports `Drench_GetPlayerGauge` (wetness, rate, thresholds, cap). It also exports the
  same call as `WFNG_GetPlayerGauge`, with the same struct.

## Logs

`Documents/My Games/Skyrim Special Edition/SKSE/Drench.log`. The Diagnostics page shows the settings source,
conflicts, the wet maps found, and every wet character.

## Credits

Built on the ideas of Wet Function Redux, OSweat and Dewpoint. The wet textures are the author's own.
Skyrim SE 1.5.97 support in WetFunction NG was contributed by GSVJinx.
The running drops are DripSim by AntiArc (https://github.com/antiarc9/DripSim), embedded with permission,
GPL-3.0-or-later: see `Drench__DripSim-LICENSE.txt`.

## License

Because it contains DripSim, this edition is released under GPL-3.0-or-later (`Drench__DripSim-LICENSE.txt`).
Its full source code is linked on the mod page (dwemermods.com/mods/6193).
