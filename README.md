# Drench - Wet Skin Framework - Running Drops edition

Characters get wet from rain, snow, water, sweat, arousal and sex, and dry off gradually. The successor of
WetFunction NG for Skyrim SE/AE: the wet look is written straight into the skin shader, with head and hand maps per
wetness state, drops in relief on UBE and - through Mu Dynamic NormalMap - on 3BA.

This edition embeds [DripSim](https://github.com/antiarc9/DripSim) by AntiArc (GPL-3.0-or-later, used with the
author's permission): water and sweat drops run down the player's skin along the live pose, drawn into a RaceMenu
overlay. Its code is in `src/drip/` (changes marked `Drench:`, origin in `src/drip/ORIGIN.txt`).
The edition without the drops is the `Drench` repository (MIT).
Mod page: https://dwemermods.com/mods/6193

This repository holds the source code. The wet textures and a ready build are in the mod archive on the mod page.

## Layout

- `src/core/` - the game-independent rules (wetness model, look, texture planning, co-save format), built as a
  static library and covered by `tests/` (doctest).
- `src/plugin/` - the SKSE plugin (C++23, CommonLibSSE-NG): materials, actors, integrations, menu, Papyrus natives.
  `Mdnm.cpp` talks to Mu Dynamic NormalMap through its published interface (MIT, Acro748).
- `src/drip/` - DripSim (GPL-3.0-or-later, AntiArc) at commit 57a5d11: the drop framework and its water and sweat
  modes, built as the static library `DrenchDrips`; `Drips.cpp` is the bridge Drench drives it through.
- `papyrus/source/` - the `Drench` script (API and console commands) and its helpers; `papyrus/compat/` - the
  `WetFunctionNG` script that forwards mods written for WetFunction NG to Drench.
- `dist_template/` - the console command list, the menu translation template and the mod readme.
- `tools/` - `build_papyrus.ps1`, `build_esp.py` (Drench.esp and the WetFunctionNG.esp stub), `stage.ps1`
  (Data layout), `package.py` (FOMOD archive and an install test), `strings_template.py`.

## Building

Requires [xmake](https://xmake.io), [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng) (alandtse's
`ng` branch) at `lib/CommonLibSSE-NG` with `commonlib-unk8c.patch` applied inside it
(`git apply ../../commonlib-unk8c.patch`: older `ng` revisions declare an extra `TESObjectREFR` virtual that SE/AE do
not have), and [SKSE-Menu-Framework-3-API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API) at
`lib/SKSE-Menu-Framework-3-API`.

```
xmake f -m releasedbg --skyrim_vr=n
xmake build Drench
xmake build tests && xmake run tests
```

`stage.ps1` and `package.py` expect the wet textures under `assets/textures/actors/character/Drench/` (take them
from the mod archive) and an optional `tools/leak_patterns.txt` with strings that must never ship.

## License

GPL-3.0-or-later, see `LICENSE` (the edition contains DripSim, which is GPL-3.0-or-later).
