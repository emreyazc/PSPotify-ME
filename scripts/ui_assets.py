"""Pinned UI fonts for PSP and PC preview.

PSPOTIFY_FONT=inter (default) or noto chooses a screen-readable family.
PSPOTIFY_FONT=mplus restores the original rounded family for comparisons.
A single variable-font download is instantiated to *static* Regular, Bold,
ExtraBold faces before subsetting, so PSP stb_truetype needs no variation
support and only the chosen family's three small faces enter the EBOOT.
All source URLs are commit-pinned and checked against Git blob digests.
"""
from pathlib import Path
import hashlib
import io
import os
import re
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / ".deps" / "fonts"
PSP_FONTS = FONTS / "psp"

FONT_FAMILY = os.environ.get("PSPOTIFY_FONT", "inter").strip().lower()
if FONT_FAMILY not in ("inter", "noto", "mplus"):
    raise ValueError("PSPOTIFY_FONT must be inter, noto or mplus")

# Pinned Google Fonts source revision (not a moving main-branch URL).
FONTS_REF = "e7db6ca603fd4862010383ff17ffd6cc4ffac1c0"
FONT_URL = f"https://raw.githubusercontent.com/google/fonts/{FONTS_REF}/ofl/"
MPLUS_REF = "84efd8ad78c3710ad14bd909e3bc407151885628"
MPLUS_URL = f"https://raw.githubusercontent.com/google/fonts/{MPLUS_REF}/ofl/"

# Inter's optical size is fixed to 14 (UI text); Noto's width to 100.
# SHA-1 values are Git blob hashes: SHA1(b"blob <length>\0" + data).
FONT_VARIANTS = {
    "inter": ("Inter-Variable.ttf",
              FONT_URL + "inter/Inter%5Bopsz%2Cwght%5D.ttf",
              "git:047c92f6e2212473dc436020afed689527076d44",
              {"opsz": 14.0}),
    "noto": ("NotoSans-Variable.ttf",
             FONT_URL + "notosans/NotoSans%5Bwdth%2Cwght%5D.ttf",
             "git:75575046c015ff623a848096a15779867ba71453",
             {"wdth": 100.0}),
}

if FONT_FAMILY == "mplus":
    DOWNLOADS = {
        "MPLUSRounded1c-Regular.ttf": (
            MPLUS_URL + "mplusrounded1c/MPLUSRounded1c-Regular.ttf",
            "b75708b53e45b06d17d470aeeca5b766e3d1b3999f03f13ec4eb863ca846c14c"),
        "MPLUSRounded1c-Bold.ttf": (
            MPLUS_URL + "mplusrounded1c/MPLUSRounded1c-Bold.ttf",
            "c358630584e8e2d8fbd6121d0f4693255ffef6d1e6d4f3441fd6e5a963a11f9e"),
        "MPLUSRounded1c-ExtraBold.ttf": (
            MPLUS_URL + "mplusrounded1c/MPLUSRounded1c-ExtraBold.ttf",
            "8e7c15901dca87f1451b356dda594f7d092ba252a5dcc47da74523a242493c36"),
        "mplus-OFL.txt": (MPLUS_URL + "mplus1p/OFL.txt",
                          "da15da6b1496d4de18f97e2ad1b722ef8a1c121149c2c93b2cf7eac6ac27b35c"),
    }
    FACE_NAMES = [
        "MPLUSRounded1c-Regular.ttf",
        "MPLUSRounded1c-Bold.ttf",
        "MPLUSRounded1c-ExtraBold.ttf",
    ]
else:
    variable_name, variable_url, variable_hash, axes = FONT_VARIANTS[FONT_FAMILY]
    DOWNLOADS = {
        variable_name: (variable_url, variable_hash),
        f"{FONT_FAMILY}-OFL.txt": (
            FONT_URL + ("inter" if FONT_FAMILY == "inter" else "notosans") + "/OFL.txt",
            "git:21f6aff961064c2e429f570995e446bcdd555422" if FONT_FAMILY == "inter"
            else "git:6843f31878c9945e1e71e1baa275546b4eefead8"),
    }
    FACE_NAMES = [variable_name] * 3

LUCIDE = ("https://registry.npmjs.org/lucide-static/-/lucide-static-1.48.0.tgz",
          "3c2ecda3d25f6a9692d83f8036d9a526f7da584a51af74cd16eda4498c5c33d8")

# Must match the slots in ui/font.c. Latin Extended-A contains Ş/ş, Ğ/ğ,
# İ/ı; Latin-1 contains Ç/ç, Ö/ö, Ü/ü.
TEXT_CODEPOINTS = (list(range(0x20, 0x7F)) + list(range(0xA0, 0x180)) +
                   [0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026,
                    0x2190, 0x2191, 0x2192, 0x2193, 0x25A1, 0x25B3, 0x25CB])
FACES = list(zip(("ui_font_regular", "ui_font_bold", "ui_font_xbold"), FACE_NAMES))
FACES.append(("ui_font_icons", "lucide.ttf"))
WEIGHTS = {"ui_font_regular": 400, "ui_font_bold": 700, "ui_font_xbold": 800}


def verify_source(data, digest):
    if digest.startswith("git:"):
        expected = digest[4:]
        check = hashlib.sha1(f"blob {len(data)}\0".encode("ascii") + data).hexdigest()
    else:
        expected = digest
        check = hashlib.sha256(data).hexdigest()
    if check != expected:
        raise RuntimeError(f"Font checksum mismatch: {check}, expected {expected}")


def download(url, digest):
    with urllib.request.urlopen(url, timeout=120) as response:
        data = response.read()
    verify_source(data, digest)
    return data


def fetch():
    FONTS.mkdir(parents=True, exist_ok=True)
    for name, (url, digest) in DOWNLOADS.items():
        target = FONTS / name
        if not target.exists():
            print(f"fetching {name}")
            target.write_bytes(download(url, digest))
        else:
            # Existing cache entries may come from another run. A pinned
            # checksum protects against stale/corrupted font assets.
            verify_source(target.read_bytes(), digest)
    icons = FONTS / "lucide.ttf"
    if not icons.exists():
        print("fetching lucide icons")
        with tarfile.open(fileobj=io.BytesIO(download(*LUCIDE)), mode="r:gz") as tar:
            icons.write_bytes(tar.extractfile("package/font/lucide.ttf").read())
            (FONTS / "lucide-LICENSE").write_bytes(tar.extractfile("package/LICENSE").read())


def icon_codepoints():
    header = (ROOT / "ui" / "icons.h").read_text(encoding="utf-8")
    return [int(cp, 16) for cp in re.findall(r"#define ICON_\w+\s+0x([0-9a-fA-F]+)u", header)]


def subset_face(symbol, name):
    """Subset the selected static font or instantiate one variable weight."""
    from fontTools import subset
    from fontTools.varLib.instancer import instantiateVariableFont

    options = subset.Options()
    options.layout_features = []
    options.hinting = False
    options.name_IDs = ["*"]
    options.notdef_outline = True
    font = subset.load_font(str(FONTS / name), options)
    if symbol != "ui_font_icons":
        # Do not silently ship a font that renders Turkish letters as '?'.
        required = "ÇçĞğİıÖöŞşÜü"
        missing = [ch for ch in required if ord(ch) not in font.getBestCmap()]
        if missing:
            raise RuntimeError(f"{name}: missing Turkish glyphs: {''.join(missing)}")
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=icon_codepoints() if symbol == "ui_font_icons"
                       else TEXT_CODEPOINTS)
    subsetter.subset(font)
    if FONT_FAMILY != "mplus" and symbol in WEIGHTS:
        # Flatten variable axes AFTER subsetting to save PSP RAM and avoid
        # variable-font support requirements in stb_truetype.
        font = instantiateVariableFont(
            font, {**axes, "wght": float(WEIGHTS[symbol])}, inplace=True)
        if "fvar" in font:
            raise RuntimeError("Variable font axes were not fully instantiated")
    buffer = io.BytesIO()
    subset.save_font(font, buffer, options)
    data = buffer.getvalue()
    print(f"{FONT_FAMILY}/{symbol}: {len(data)} bytes after subsetting")
    return data


def psp_fonts():
    """Subsets the chosen family into C arrays linked inside the EBOOT."""
    fetch()
    PSP_FONTS.mkdir(parents=True, exist_ok=True)
    out = PSP_FONTS / "ui_fonts.c"
    lines = [f"/* Generated by scripts/ui_assets.py: PSPOTIFY_FONT={FONT_FAMILY}. */", ""]
    for symbol, name in FACES:
        data = subset_face(symbol, name)
        lines.append(f"const unsigned char {symbol}[{len(data)}] __attribute__((aligned(4))) = {{")
        for i in range(0, len(data), 24):
            lines.append("    " + ",".join(str(b) for b in data[i:i + 24]) + ",")
        lines.append("};")
        lines.append(f"const unsigned int {symbol}_len = {len(data)};")
        lines.append("")
    out.write_text("\n".join(lines), encoding="utf-8")
    return out
