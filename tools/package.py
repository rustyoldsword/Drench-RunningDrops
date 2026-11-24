r"""Pack dist\Drench (made by tools\stage.ps1) into the release archive: a FOMOD with an explicit Main mod page.

  core/    everything else (always installed): the plugin, the 3BA / CBBE, male, head, COTR head and ear maps
  compat/  WetFunctionNG.esp (empty ESL stub) + the forwarding WetFunctionNG script (recommended option)
  relief/  the 3BA relief maps for Mu Dynamic NormalMap (one of the female body options)
  bhunp/   the female body and hand maps in the BHUNP / UNP layout, under the 3BA maps' names (tools\make_bhunp_maps.py;
           not part of dist\Drench, taken from assets_bhunp) - they replace core's 3BA ones (one of the body options)
  ube/     every UBE map, female and male (an option of its own, recommended only when UBE_AllRace.esp is active)
  fomod/   info.xml, ModuleConfig.xml - UTF-8 without an XML declaration (Vortex chokes on ElementTree's one)
COTR head maps stay in core on purpose: NPC replacers bake COTR heads into their FaceGen, often without the user
knowing it, so nobody should have to pick them.

Then the install path is tested the way a user meets it: the archive is extracted into an empty folder, the
installer's choices are read from ModuleConfig.xml and carried out, and the result must equal dist\Drench file for
file (MD5), with the build-machine leak check run over the extracted copy too.

Output: <Desktop>\Drench - Wet Skin Framework <version>.7z.   Usage:  python tools\package.py
Re-running rebuilds the archive from scratch and gives the same files."""
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "dist" / "Drench"
OUT = ROOT / "dist" / "fomod"
VERSION = re.search(r'set_version\("([^"]+)"\)', (ROOT / "xmake.lua").read_text(encoding="utf-8")).group(1)
NAME = "Drench - Wet Skin Framework"
EDITION = "Running Drops"  # the edition with the embedded DripSim (GPL-3.0-or-later); 1.0.1 without it stays the plain main file
ARCHIVE = Path.home() / "Desktop" / f"{NAME} {VERSION} - {EDITION}.7z"
SEVEN_ZIP = next(p for p in (Path(r"C:\Program Files\7-Zip\7z.exe"), Path(r"C:\Program Files (x86)\7-Zip\7z.exe")) if p.exists())
COMPAT = {Path("WetFunctionNG.esp"), Path("Scripts/WetFunctionNG.pex"), Path("Scripts/Source/WetFunctionNG.psc")}
MDNM_OPTION = "3BA (CBBE) with Mu Dynamic NormalMap - drops in relief (best)"
PLAIN_OPTION = "3BA (CBBE) without Mu Dynamic NormalMap - wet shine only"
BHUNP_OPTION = "BHUNP / UNP - wet shine only"
UBE_OPTION = "UBE bodies"
MAPS = Path("textures/actors/character/Drench")
BHUNP = ROOT / "assets_bhunp"


def is_relief(rel):
    """The 3BA relief maps for Mu Dynamic NormalMap (tools\\make_mdnm_detail.py): wet_<state>_detail_n.dds."""
    return rel.parent == MAPS and rel.name.endswith("_detail_n.dds")


def is_ube(rel):
    """A UBE map: wet_*_UBE_n, wethand/wethead*_UBE_n, male_*_UBE_n, wetear_*_UBE_s ..."""
    return rel.parent == MAPS and "_ube_" in rel.name.lower()


def folder_of(rel):
    return "compat" if rel in COMPAT else "relief" if is_relief(rel) else "ube" if is_ube(rel) else "core"
# strings that must never ship (build machine names, local paths): one per line in tools\leak_patterns.txt
_PATTERNS = Path(__file__).with_name("leak_patterns.txt")
LEAKS = [re.compile(line.strip().encode(), re.I) for line in (_PATTERNS.read_text(encoding="utf-8").splitlines() if _PATTERNS.exists() else []) if line.strip()]


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def tree(root):
    return {p.relative_to(root): p for p in root.rglob("*") if p.is_file()}


def module_config():
    root = ET.Element("config")
    root.set("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance")
    root.set("xsi:noNamespaceSchemaLocation", "http://qconsulting.ca/fo3/ModConfig5.0.xsd")
    ET.SubElement(root, "moduleName").text = f"{NAME} - {EDITION}"
    ET.SubElement(ET.SubElement(root, "requiredInstallFiles"), "folder", source="core", destination="", priority="0")
    steps = ET.SubElement(root, "installSteps", order="Explicit")
    step = ET.SubElement(steps, "installStep", name="Main mod")
    groups = ET.SubElement(step, "optionalFileGroups", order="Explicit")

    main = ET.SubElement(groups, "group", name="Main mod", type="SelectAll")
    plugins = ET.SubElement(main, "plugins", order="Explicit")
    plugin = ET.SubElement(plugins, "plugin", name=f"Drench - Wet Skin Framework {VERSION} - {EDITION}")
    ET.SubElement(plugin, "description").text = (
        "Wetness from rain, snow, water, sweat, arousal and sex, written straight into the skin shader: wet maps on "
        "body, hands, feet, head, Goam ears and genitals, female and male, including COTR heads (NPC replacers often "
        "bake them in). Settings in SKSE Menu Framework. Always installed. Pick your female body and UBE below. "
        "Running Drops edition: water and sweat drops run down the player's skin (embedded DripSim, GPL-3.0; needs "
        "RaceMenu and a free body overlay slot). Disable WetFunction NG, Wet Function Redux, OSweat, Dewpoint and DripSim.")
    ET.SubElement(plugin, "files")  # the files themselves are the required core folder
    ET.SubElement(ET.SubElement(plugin, "typeDescriptor"), "type", name="Required")

    compat = ET.SubElement(groups, "group", name="Compatibility", type="SelectAny")
    plugins = ET.SubElement(compat, "plugins", order="Explicit")
    plugin = ET.SubElement(plugins, "plugin", name="WetFunction NG compatibility")
    ET.SubElement(plugin, "description").text = (
        "For mods written for WetFunction NG (SexLab Survival, Diary of Mine, Vitals HUD, SexLab P++ and others): "
        "an empty ESL WetFunctionNG.esp, so their checks find it, and a WetFunctionNG script that hands every call "
        "to Drench. Recommended. Never install it next to the real WetFunction NG.")
    files = ET.SubElement(plugin, "files")
    ET.SubElement(files, "folder", source="compat", destination="", priority="0")
    ET.SubElement(ET.SubElement(plugin, "typeDescriptor"), "type", name="Recommended")

    # The female body the game uses (one per game): 3BA with relief through Mu Dynamic NormalMap, 3BA shine only, or
    # BHUNP / UNP (its own UV layout: its maps replace the 3BA ones under the same names). The installer recommends
    # the relief when MuDynamicNormalMap.dll is installed, 3BA shine otherwise; BHUNP is never pre-picked.
    body = ET.SubElement(groups, "group", name="Female body", type="SelectExactlyOne")
    plugins = ET.SubElement(body, "plugins", order="Explicit")
    for name, description, folder, kind_default, kind_with_mdnm in (
        (MDNM_OPTION,
         "Best quality. Drops and streaks stand up on the 3BA body in real relief, like on UBE, on top of the shine. "
         "Needs Mu Dynamic NormalMap: Drench hands it the drops as a detail map, and it rebuilds the body's normal map "
         "from its real shape. Adds 4K relief maps.", "relief", "Optional", "Recommended"),
        (PLAIN_OPTION,
         "3BA / CBBE without Mu Dynamic NormalMap. The body gets the wet shine maps only - drops and sweat drawn in "
         "the shine, close to WetFunction NG.", None, "Recommended", "Optional"),
        (BHUNP_OPTION,
         "BHUNP, UUNP or UNP bodies (the UNP UV layout). The wet body and hand maps are redrawn for that layout and "
         "replace the 3BA ones. Wet shine only, no relief. Do not pick it for 3BA / CBBE bodies.", "bhunp", "Optional",
         "Optional"),
    ):
        plugin = ET.SubElement(plugins, "plugin", name=name)
        ET.SubElement(plugin, "description").text = description
        files = ET.SubElement(plugin, "files")
        if folder:
            ET.SubElement(files, "folder", source=folder, destination="", priority="1")
        kind = ET.SubElement(ET.SubElement(plugin, "typeDescriptor"), "dependencyType")
        ET.SubElement(kind, "defaultType", name=kind_default)
        pattern = ET.SubElement(ET.SubElement(kind, "patterns"), "pattern")
        deps = ET.SubElement(pattern, "dependencies", operator="And")
        ET.SubElement(deps, "fileDependency", file="SKSE\\Plugins\\MuDynamicNormalMap.dll", state="Active")
        ET.SubElement(pattern, "type", name=kind_with_mdnm)

    # UBE: only for games that have it - recommended when UBE's plugin is active, a free choice otherwise
    ube = ET.SubElement(groups, "group", name="UBE", type="SelectAny")
    plugins = ET.SubElement(ube, "plugins", order="Explicit")
    plugin = ET.SubElement(plugins, "plugin", name=UBE_OPTION)
    ET.SubElement(plugin, "description").text = (
        "Wet maps for UBE (Unified Body Enhancement) women and men: body, hands, feet and head with drops in relief, "
        "and the Goam ear maps for UBE heads. Only needed if your game has UBE characters. Without it a UBE "
        "character still gets the wet shine.")
    files = ET.SubElement(plugin, "files")
    ET.SubElement(files, "folder", source="ube", destination="", priority="0")
    kind = ET.SubElement(ET.SubElement(plugin, "typeDescriptor"), "dependencyType")
    ET.SubElement(kind, "defaultType", name="Optional")
    pattern = ET.SubElement(ET.SubElement(kind, "patterns"), "pattern")
    deps = ET.SubElement(pattern, "dependencies", operator="And")
    ET.SubElement(deps, "fileDependency", file="UBE_AllRace.esp", state="Active")
    ET.SubElement(pattern, "type", name="Recommended")
    return ET.ElementTree(root)


def info_xml():
    root = ET.Element("fomod")
    ET.SubElement(root, "Name").text = f"{NAME} - {EDITION}"
    ET.SubElement(root, "Version").text = VERSION
    ET.SubElement(root, "Description").text = (
        "Characters get wet from rain, snow, water, sweat, arousal and sex and dry off gradually. The successor of "
        "WetFunction NG: wet maps written straight into the skin shader. Running Drops edition: drops run down the player's "
        "skin (embedded DripSim, needs RaceMenu).")
    ET.SubElement(ET.SubElement(root, "Groups"), "element").text = "SKSE"
    return ET.ElementTree(root)


def write_xml(tree, path):
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(tree, space="  ")
    tree.write(path, encoding="utf-8", xml_declaration=False)
    ET.parse(path)


def install(extracted, target, body, ube):
    """What a FOMOD installer does: the required folder, every plugin typed Required/Recommended, the female body
    option picked (body: MDNM_OPTION / PLAIN_OPTION / BHUNP_OPTION) and UBE or not; folders in priority order, so
    a higher-priority option's files replace core's."""
    config = ET.parse(extracted / "fomod" / "ModuleConfig.xml").getroot()
    folders = [(int(f.get("priority", 0)), f.get("source"), f.get("destination")) for f in config.find("requiredInstallFiles")]
    for plugin in config.iter("plugin"):
        plain = plugin.find("typeDescriptor/type")
        if plain is not None:
            chosen = plain.get("name") in ("Required", "Recommended")
        elif plugin.get("name") == UBE_OPTION:
            chosen = ube
        else:
            chosen = plugin.get("name") == body
        if chosen:
            folders += [(int(f.get("priority", 0)), f.get("source"), f.get("destination")) for f in plugin.find("files")]
    for _, source, destination in sorted(folders, key=lambda f: f[0]):
        shutil.copytree(extracted / source, target / destination, dirs_exist_ok=True)
    return config


def leaks(root):
    found = []
    for rel, path in tree(root).items():
        if path.suffix.lower() == ".dds":
            continue
        data = path.read_bytes()
        found += [f"{rel}: {m.group(0)!r}" for pattern in LEAKS for m in [pattern.search(data)] if m]
    return found


def main():
    global ARCHIVE
    if "--archive" in sys.argv:  # a trial build elsewhere (the Desktop archive is made only for a release)
        ARCHIVE = Path(sys.argv[sys.argv.index("--archive") + 1])
    if not STAGE.exists():
        sys.exit("dist\\Drench is missing: run tools\\stage.ps1")
    staged = tree(STAGE)
    assert COMPAT <= set(staged), COMPAT - set(staged)
    if OUT.exists():
        shutil.rmtree(OUT)
    for rel, path in staged.items():
        dst = OUT / folder_of(rel) / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dst)
    bhunp = tree(BHUNP)
    assert bhunp and all(rel in staged for rel in bhunp), f"BHUNP maps must replace 3BA ones: {set(bhunp) - set(staged)}"
    for rel, path in bhunp.items():
        dst = OUT / "bhunp" / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, dst)
    write_xml(module_config(), OUT / "fomod" / "ModuleConfig.xml")
    write_xml(info_xml(), OUT / "fomod" / "info.xml")
    for xml in (OUT / "fomod").iterdir():
        assert not xml.read_bytes().startswith((b"<?xml", b"\xef\xbb\xbf")), f"{xml.name} has a declaration or BOM"

    if ARCHIVE.exists():
        ARCHIVE.unlink()
    subprocess.run([str(SEVEN_ZIP), "a", "-t7z", "-mx=9", "-md=256m", "-mmt=on", str(ARCHIVE), "."], cwd=OUT, check=True,
                   stdout=subprocess.DEVNULL)

    # the user's path: extract, run the installer's choices, compare with the build
    with tempfile.TemporaryDirectory() as tmp:
        extracted, installed = Path(tmp) / "archive", Path(tmp) / "Data"
        subprocess.run([str(SEVEN_ZIP), "x", str(ARCHIVE), f"-o{extracted}"], check=True, stdout=subprocess.DEVNULL)
        assert set(tree(extracted)) == set(tree(OUT)), "the archive does not hold what was packed"
        relief = {rel for rel in staged if is_relief(rel)}
        ube = {rel for rel in staged if is_ube(rel)}
        assert relief and ube, "no relief or no UBE maps staged"
        cotr = {rel for rel in staged if "_cotr" in rel.name.lower()}
        assert cotr, "no COTR head maps staged"
        everything = set(staged)
        cases = (
            (MDNM_OPTION, True, everything, {}),
            (MDNM_OPTION, False, everything - ube, {}),
            (PLAIN_OPTION, True, everything - relief, {}),
            (PLAIN_OPTION, False, everything - relief - ube, {}),
            (BHUNP_OPTION, True, everything - relief, bhunp),
            (BHUNP_OPTION, False, everything - relief - ube, bhunp),
        )
        for n, (body, with_ube, expected, replaced) in enumerate(cases):
            installed = Path(tmp) / f"Data-{n}"
            install(extracted, installed, body, with_ube)
            got = tree(installed)
            label = f"{body} / UBE {with_ube}"
            assert set(got) == expected, f"{label}: missing {expected - set(got)}, extra {set(got) - expected}"
            want = {rel: (replaced.get(rel) or staged[rel]) for rel in expected}
            bad = [str(rel) for rel in expected if md5(want[rel]) != md5(got[rel])]
            assert not bad, f"{label}: differ after install: {bad}"
            assert cotr <= set(got), f"{label}: COTR maps missing"
        problems = leaks(extracted)
        assert not problems, f"build-machine text in the archive: {problems}"
        core_only = set(tree(extracted / "core"))
        assert not (COMPAT & core_only), "compatibility files leaked into core"
        assert not any(is_relief(rel) or is_ube(rel) for rel in core_only), "relief or UBE maps leaked into core"

    print(f"{len(staged)} files ({len(COMPAT)} compatibility, {len(relief)} 3BA relief, {len(ube)} UBE, {len(cotr)} COTR in core),"
          f" {len(bhunp)} BHUNP replacements; install test of all 6 choices: equal to the build, no leaks")
    print(f"{ARCHIVE.name}: {ARCHIVE.stat().st_size:,} bytes, MD5 {md5(ARCHIVE).upper()}")
    print(f"Drench.dll MD5 {md5(STAGE / 'SKSE' / 'Plugins' / 'Drench.dll').upper()}")


if __name__ == "__main__":
    main()
