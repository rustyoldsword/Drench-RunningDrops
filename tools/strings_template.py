r"""Collect every menu text of src\plugin\Menu.cpp into a translation file.

  python tools\strings_template.py                       -> dist_template\SKSE\Plugins\Drench\Strings_TEMPLATE.ini
  python tools\strings_template.py --check <file.ini>    -> lists texts the file lacks and lines it has that no longer exist

A translator copies the template to Strings_<LANGUAGE>.ini (LANGUAGE as the game's sLanguage, e.g. RUSSIAN) and
writes each translation after " = ". Texts left as they are show in English. Re-running gives the same file."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MENU = ROOT / "src" / "plugin" / "Menu.cpp"
OUT = ROOT / "dist_template" / "SKSE" / "Plugins" / "Drench" / "Strings_TEMPLATE.ini"

# calls whose string arguments are shown to the player (format strings and ImGui ids are left out below)
SHOWN = re.compile(r"\b(T|Toggle|Slider|Heading|Wrapped|Button|Help|offer)\(|const char\* const \w+\[\]")
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
EXTRA = ["dry", "sweat", "soaked"]  # phase names, shown through T() at run time


def unescape(text):
    return text.replace('\\"', '"').replace("\\\\", "\\")


def texts():
    seen = []
    for line in MENU.read_text(encoding="utf-8").splitlines():
        if not SHOWN.search(line) or "logger::" in line or "Visuals::Out" in line:
            continue
        for raw in LITERAL.findall(line):
            text = unescape(raw)
            if len(text.strip()) < 2 or "%" in text or "{}" in text or text.startswith("#") or text in ("Drench", "##mode"):
                continue
            if text not in seen:
                seen.append(text)
    for text in EXTRA:
        if text not in seen:
            seen.append(text)
    return seen


def read(path):
    pairs = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if not line or line[0] in ";#[" or " = " not in line:
            continue
        english, translation = line.split(" = ", 1)
        pairs[english.strip()] = translation.strip()
    return pairs


def main():
    all_texts = texts()
    if len(sys.argv) > 2 and sys.argv[1] == "--check":
        have = read(sys.argv[2])
        missing = [t for t in all_texts if t not in have]
        stale = [t for t in have if t not in all_texts]
        print(f"{len(all_texts)} texts, {len(have)} in the file, {len(missing)} missing, {len(stale)} no longer used")
        for t in missing:
            print("  missing:", t)
        for t in stale:
            print("  unused: ", t)
        sys.exit(1 if missing else 0)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "; Drench - menu texts. Copy to Strings_<LANGUAGE>.ini (the game's sLanguage, e.g. Strings_RUSSIAN.ini)",
        "; and write the translation after \" = \". Keep the English side exactly as it is. \\n is a line break.",
        "; Lines left untranslated show in English.",
        "",
    ]
    lines += [f"{t} = {t}" for t in all_texts]
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{len(all_texts)} texts -> {OUT}")


if __name__ == "__main__":
    main()
