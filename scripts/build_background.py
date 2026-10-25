"""Build the standalone ARK-5 GAME PRX and stage a test folder.

Do not touch PSPotify EBOOT or install plugins automatically on a PSP.
Run inside pspdev/pspdev:latest from the repository root.
"""
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "background"
DIST = ROOT / "dist" / "background"


def main():
    subprocess.run(["make", "-C", str(SOURCE), "all"], check=True)
    prx = SOURCE / "pspotify_bg.prx"
    if not prx.is_file() or prx.stat().st_size < 100:
        raise RuntimeError("pspotify_bg.prx was not produced")
    DIST.mkdir(parents=True, exist_ok=True)
    shutil.copy2(prx, DIST / "pspotify_bg.prx")
    shutil.copy2(SOURCE / "config.txt", DIST / "config.txt")
    generator = str(ROOT / "scripts" / "create_background_sample.py")
    # Keep legacy single-file playback, plus three distinct test songs.
    for name, pitch in (("music.wav", "1.0"),
                        ("music01.wav", "1.0"),
                        ("music02.wav", "1.25"),
                        ("music03.wav", "1.5")):
        subprocess.run([
            sys.executable, generator,
            "--output", str(DIST / name), "--pitch", pitch,
        ], check=True)
    # Safe starting state. Never overwrite a user's enabled.txt automatically.
    (DIST / "enabled-default-off.txt").write_text("0\n", encoding="ascii")
    print("BACKGROUND PRX BUILT (not PSP-tested)")
    print("Files in:", DIST)


if __name__ == "__main__":
    main()
