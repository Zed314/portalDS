#!/usr/bin/env python3
"""Generates the portal gun's refused-shot sound.

Every other effect in this folder was recorded; this one is synthesised, so the
recipe lives here rather than leaving an unexplained binary in the tree. Re-run
it from the repository root to rebuild both outputs:

    python3 sfx/portalgun_refused.gen.py

It writes the editable source next to the other .wav files and the raw PCM the
ROM actually loads into nitrofiles.

The brief was "not aggressive": two soft tones a fourth apart, falling rather
than rising, and mixed to sit clearly under the firing sounds. A buzzer
would carry the same information and be unpleasant to trigger repeatedly, which
a player pointing the gun at a non-portalable wall will do a great deal. Sine
with a trace of second harmonic keeps it warm instead of beepy, and every edge
is raised-cosine so nothing clicks.

The game plays samples through soundPlaySample() at a fixed 22050 Hz, 16 bit
signed mono - see playSFX() in arm9/source/game/sfx.c - so that is what comes
out regardless of what is set below.
"""

import math
import os
import struct
import wave

RATE = 22050          # playSFX() hardcodes this
# Set by loudness, not by peak. A gunshot is a transient - it hits 0.95 of full
# scale but only carries 16.5% RMS - whereas a sustained sine sits near its own
# peak the whole time. Matching peaks would have made this the louder of the
# two. 0.18 puts it around 10% RMS, comfortably under the shot it follows.
PEAK = 0.18
HARMONIC = 0.12       # a little second harmonic, to stop it sounding like a beep

# A descending perfect fourth: C5 down to G4. Falling reads as "no" where the
# same two tones rising would read as "ready".
TONES = [(523.25, 0.075), (392.00, 0.115)]
OVERLAP = 0.015       # seconds of crossfade, so the step glides
ATTACK = 0.006
RELEASE = 0.045


def raised_cosine(x):
    """0 to 1 with zero slope at both ends, which is what avoids the click."""
    return 0.5 - 0.5 * math.cos(math.pi * min(max(x, 0.0), 1.0))


def envelope(t, duration):
    a = raised_cosine(t / ATTACK) if t < ATTACK else 1.0
    remaining = duration - t
    r = raised_cosine(remaining / RELEASE) if remaining < RELEASE else 1.0
    return a * r


def render():
    total = sum(d for _, d in TONES) - OVERLAP * (len(TONES) - 1)
    samples = [0.0] * int(total * RATE)

    start = 0.0
    for freq, duration in TONES:
        base = int(start * RATE)
        for i in range(int(duration * RATE)):
            if base + i >= len(samples):
                break
            t = i / RATE
            phase = 2.0 * math.pi * freq * t
            value = math.sin(phase) + HARMONIC * math.sin(2.0 * phase)
            samples[base + i] += value * envelope(t, duration) / (1.0 + HARMONIC)
        start += duration - OVERLAP

    # The crossfade can push two tones past the peak; scale to fit rather than
    # clip, so the shape stays the shape.
    loudest = max(abs(s) for s in samples)
    gain = PEAK / loudest if loudest else 0.0
    return [int(max(-32768, min(32767, round(s * gain * 32767)))) for s in samples]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)

    pcm = render()
    blob = struct.pack("<%dh" % len(pcm), *pcm)

    raw = os.path.join(root, "nitrofiles", "asds", "sfx", "portalgun_refused.raw")
    with open(raw, "wb") as f:
        f.write(blob)

    src = os.path.join(here, "portalgun_refused.wav")
    with wave.open(src, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(blob)

    print("%d samples, %.0f ms" % (len(pcm), 1000.0 * len(pcm) / RATE))
    print("  %s" % os.path.relpath(raw, root))
    print("  %s" % os.path.relpath(src, root))


if __name__ == "__main__":
    main()
