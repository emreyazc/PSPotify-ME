"""Create an original, copyright-free 10-second music sample for PSP BG.

Output: signed 16-bit stereo PCM WAV at 44,100 Hz; no extra libraries needed.
Replace this file with any *owned/licensed* PCM16 44.1kHz WAV after the test.
"""
from pathlib import Path
import argparse
import math
import struct
import wave

RATE = 44100
NOTES = (220, 261.63, 329.63, 293.66, 392, 329.63, 261.63, 220)
SECS_PER_NOTE = 1.2


def make_sample(target: Path, pitch: float = 1.0):
    target.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(target), "wb") as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(RATE)
        frames_per_note = round(RATE * SECS_PER_NOTE)
        for original_note in NOTES:
            note = original_note * pitch
            buffer = bytearray()
            for i in range(frames_per_note):
                time = i / RATE
                envelope = min(1.0, i / (0.025 * RATE),
                               (frames_per_note - i) / (0.075 * RATE))
                # Simple melody with low-amplitude harmonics, no samples.
                sample = (math.sin(math.tau * note * time) * 0.75 +
                          math.sin(math.tau * note * 2 * time) * 0.16 +
                          math.sin(math.tau * note * 3 * time) * 0.09)
                value = int(5000 * max(0.0, envelope) * sample)
                buffer.extend(struct.pack("<hh", value, value))
            out.writeframesraw(buffer)
    print(f"WAV READY: {target} ({target.stat().st_size} bytes)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("dist/background/music.wav"))
    parser.add_argument("--pitch", type=float, default=1.0,
                        help="Change note frequencies to distinguish playlist tracks")
    args = parser.parse_args()
    make_sample(args.output, args.pitch)
