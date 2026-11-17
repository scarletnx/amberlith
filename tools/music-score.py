#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turns the demos' scores, source/musicscore.txt, into the tables the composer reads, source/musicscore.c.

    tools/music-score.py [--check] [SCORE [OUT]]

The score is what each track is made of (see the comment at its top); the C file is its tables, generated, never
edited by hand. Every track must be one musiccompose.h names, in its order; every field must be there once and in
range; nine channels with at most one drums, bass, lead, arp and crash and three pads, and no echo, double or
harmony without a lead; a progression's degrees must be triads that are not diminished in the track's mode; every
pattern must be as long as its kind and use only its marks, and a pad pattern the pads glide out of must not let the
chord go on its last row (the glide would start from silence). A score that breaks one is refused with the line,
what it holds and what it should.

--check writes nothing: it says whether OUT is what SCORE makes, and exits 1 when it is not.
"""

import argparse
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TRACKS = ["menu", "wimsy", "fractal", "fluid"]
MODES = ["ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian"]
MODE_STEPS = {
    "ionian": [0, 2, 4, 5, 7, 9, 11],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "aeolian": [0, 2, 3, 5, 7, 8, 10],
}
ROLES = ["drums", "bass", "pad", "lead", "echo", "double", "harmony", "arp", "crash"]
ROLE_MAX = {"drums": 1, "bass": 1, "pad": 3, "lead": 1, "echo": 1, "double": 1, "harmony": 1, "arp": 1, "crash": 1}
VOICES = ["kick", "snare", "hat", "crash", "riser", "bass", "pad", "lead", "arp"]
RANGES = ["bass", "pad", "lead", "arp"]
PITCH = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
# Each pool: (the C enum's name, its length, the marks it takes, the fewest entries a track gives).
POOLS = {
    "form": ("FORM", None, set("IABRC"), 1),
    "home": ("HOME", 4, set("1234567"), 1),
    "lift": ("LIFT", 4, set("1234567"), 1),
    "drop": ("DROP", 4, set("1234567"), 1),
    "groove": ("GROOVE", 16, set("KSshH."), 1),
    "fill": ("FILL", 16, set("KSshH."), 0),
    "intro": ("INTRO", 16, set("KSshH."), 0),
    "breakbeat": ("BREAKBEAT", 16, set("KSshH."), 1),
    "bass": ("BASS", 16, set("RO357Aroa-."), 1),
    "breakbass": ("BREAKBASS", 16, set("RO357Aroa-."), 1),
    "pad": ("PAD", 16, set("x-."), 1),
    "climax": ("CLIMAX", 16, set("c-."), 0),
    "arp": ("ARP", 16, set("123456789-."), 0),
    "rhythm": ("RHYTHM", 32, set("x-."), 2),
}
FIELDS = ["demo", "key", "tempo", "gain", "channels", "voices", "echo", "glide"]


class ScoreError(Exception):
    pass


def note(token, where):
    m = re.fullmatch(r"([A-G])([#b]?)(-?[0-8])", token)
    if not m:
        raise ScoreError(f"{where}: {token!r} is no note: a letter from A to G, # or b or nothing, an octave (E2)")
    midi = 12 * (int(m.group(3)) + 1) + PITCH[m.group(1)] + {"#": 1, "b": -1, "": 0}[m.group(2)]
    if not 12 <= midi <= 108:
        raise ScoreError(f"{where}: {token!r} is MIDI note {midi}, where 12 to 108")
    return midi


def number(token, low, high, what, where):
    if not re.fullmatch(r"-?[0-9]+", token):
        raise ScoreError(f"{where}: {what} {token!r} is not a whole number from {low} to {high}")
    value = int(token)
    if not low <= value <= high:
        raise ScoreError(f"{where}: {what} {value} is outside {low} to {high}")
    return value


def diminished(mode, degree):
    steps = MODE_STEPS[mode]
    root = steps[degree]
    return (steps[(degree + 4) % 7] - root) % 12 == 6


def parse(text):
    tracks = []
    track = None
    for number_, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        where = f"line {number_}"
        if not line or line.startswith("#"):
            continue
        word, _, rest = line.partition(" ")
        rest = rest.strip()
        if word == "track":
            if not rest:
                raise ScoreError(f"{where}: a track needs a name")
            track = {"name": rest, "line": number_, "pools": {k: [] for k in POOLS}}
            tracks.append(track)
            continue
        if track is None:
            raise ScoreError(f"{where}: {word!r} before any track")
        if word in POOLS:
            enum, length, marks, _ = POOLS[word]
            entry = rest.replace(" ", "") if word in ("home", "lift", "drop") else rest
            if length is not None and len(entry) != length:
                raise ScoreError(f"{where}: {word} {entry!r} is {len(entry)} long, where {length}")
            if word == "form" and not 1 <= len(entry) <= 8:
                raise ScoreError(f"{where}: form {entry!r} has {len(entry)} sections, where 1 to 8")
            bad = sorted(set(entry) - marks)
            if bad:
                raise ScoreError(f"{where}: {word} {entry!r} has {''.join(bad)!r}, where only {''.join(sorted(marks))!r}")
            track["pools"][word].append((entry, number_))
            continue
        if word not in FIELDS + ["range"]:
            raise ScoreError(f"{where}: {word!r} is no field: {', '.join(FIELDS + ['range'] + list(POOLS))}")
        if word == "range":
            parts = rest.split()
            if len(parts) != 3 or parts[0] not in RANGES:
                raise ScoreError(f"{where}: range {rest!r}: want PART LOW HIGH, PART one of {', '.join(RANGES)}")
            low, high = note(parts[1], where), note(parts[2], where)
            if high - low < 12:
                raise ScoreError(f"{where}: range {parts[0]} runs from {low} to {high}, less than the octave the composer folds notes by")
            ranges = track.setdefault("range", {})
            if parts[0] in ranges:
                raise ScoreError(f"{where}: range {parts[0]} given twice")
            ranges[parts[0]] = (low, high)
            continue
        if word in track:
            raise ScoreError(f"{where}: {word} given twice in track {track['name']}")
        track[word] = (rest, number_)
    return tracks


def check(tracks):
    names = [t["name"] for t in tracks]
    if names != TRACKS:
        raise ScoreError(f"the tracks are {names}, where {TRACKS} in that order (musiccompose.h's MusicTrack)")
    out = []
    for t in tracks:
        where = f"track {t['name']} (line {t['line']})"
        for field in FIELDS:
            if field not in t:
                raise ScoreError(f"{where}: no {field}")
        missing = [r for r in RANGES if r not in t.get("range", {})]
        if missing:
            raise ScoreError(f"{where}: no range for {', '.join(missing)}")
        key = t["key"][0].split()
        if len(key) != 2 or key[1] not in MODES:
            raise ScoreError(f"line {t['key'][1]}: key {t['key'][0]!r}: want NOTE MODE, MODE one of {', '.join(MODES)}")
        tonic = note(key[0] + "4", f"line {t['key'][1]}") % 12
        mode = key[1]
        tempo = t["tempo"][0].split()
        if len(tempo) != 2:
            raise ScoreError(f"line {t['tempo'][1]}: tempo {t['tempo'][0]!r}: want QUARTERS_A_MINUTE TICKS_A_ROW")
        bpm = number(tempo[0], 60, 200, "tempo", f"line {t['tempo'][1]}")
        speed = number(tempo[1], 1, 16, "ticks a row", f"line {t['tempo'][1]}")
        gain = number(t["gain"][0], 1, 64, "gain", f"line {t['gain'][1]}")
        roles = t["channels"][0].split()
        if len(roles) != 9:
            raise ScoreError(f"line {t['channels'][1]}: {len(roles)} channels, where 9")
        for r in roles:
            if r not in ROLES:
                raise ScoreError(f"line {t['channels'][1]}: {r!r} is no part: {', '.join(ROLES)}")
        for r, most in ROLE_MAX.items():
            if roles.count(r) > most:
                raise ScoreError(f"line {t['channels'][1]}: {roles.count(r)} channels play {r}, at most {most}")
        for r in ("echo", "double", "harmony"):
            if r in roles and "lead" not in roles:
                raise ScoreError(f"line {t['channels'][1]}: {r} follows the lead, and no channel plays one")
        voices = {}
        for item in t["voices"][0].split():
            name, _, patches = item.partition("=")
            choices = patches.split(",")
            if name not in VOICES or not patches or len(choices) > 2 or "" in choices:
                raise ScoreError(f"line {t['voices'][1]}: {item!r}: want VOICE=PATCH[,PATCH], VOICE one of {', '.join(VOICES)}")
            voices[name] = choices
        missing = [v for v in VOICES if v not in voices]
        if missing:
            raise ScoreError(f"line {t['voices'][1]}: no patch for {', '.join(missing)}")
        echo = t["echo"][0].split()
        if len(echo) != 2:
            raise ScoreError(f"line {t['echo'][1]}: echo {t['echo'][0]!r}: want ROWS SOFTER")
        echo_rows = number(echo[0], 1, 8, "echo rows", f"line {t['echo'][1]}")
        echo_drop = number(echo[1], 0, 40, "echo's drop", f"line {t['echo'][1]}")
        glide = number(t["glide"][0], 0, 127, "glide", f"line {t['glide'][1]}")
        for pool, (_, _, _, fewest) in POOLS.items():
            if len(t["pools"][pool]) < fewest:
                raise ScoreError(f"{where}: {len(t['pools'][pool])} {pool} entries, at least {fewest}")
        if "arp" in roles and not t["pools"]["arp"]:
            raise ScoreError(f"{where}: a channel plays the arp and no arp pattern is given")
        if glide:
            for entry, line in t["pools"]["pad"]:
                if entry.endswith("."):
                    raise ScoreError(
                        f"line {line}: pad {entry!r} lets the chord go on its last row, and glide {glide} "
                        "slides into the next one from it: end on - or x, or glide 0"
                    )
        for entry, line in t["pools"]["form"]:
            if "A" not in entry:
                raise ScoreError(f"line {line}: form {entry!r} has no A section")
            if "I" in entry[1:]:
                raise ScoreError(f"line {line}: form {entry!r} has an intro past its start")
        for pool in ("home", "lift", "drop"):
            for entry, line in t["pools"][pool]:
                for d in entry:
                    if diminished(mode, int(d) - 1):
                        raise ScoreError(f"line {line}: {pool} {entry!r}: degree {d} is a diminished triad in {mode}")
        out.append(
            {
                "name": t["name"],
                "demo": t["demo"][0],
                "tonic": tonic,
                "mode": mode,
                "tempo": bpm,
                "speed": speed,
                "gain": gain,
                "roles": roles,
                "voices": voices,
                "range": t["range"],
                "echo_rows": echo_rows,
                "echo_drop": echo_drop,
                "glide": glide,
                "pools": {k: [e for e, _ in v] for k, v in t["pools"].items()},
            }
        )
    return out


def c_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(scores):
    lines = [
        "// SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "",
        "// Generated by tools/music-score.py from source/musicscore.txt: edit that file, not this one.",
        "",
        '#include "musicscore.h"',
        "",
    ]
    for s in scores:
        for pool, entries in s["pools"].items():
            if not entries:
                continue
            lines.append(f"static const char *const {s['name'].upper()}_{POOLS[pool][0]}[] = {{")
            for e in entries:
                lines.append(f"    {c_string(e)},")
            lines.append("};")
        lines.append("")
    lines.append("const MusicScore MUSIC_SCORES[] = {")
    for s in scores:
        lines += [
            "    {",
            f"        .name = {c_string(s['name'])},",
            f"        .demo = {c_string(s['demo'])},",
            f"        .tonic = {s['tonic']},",
            f"        .mode = MUSIC_{s['mode'].upper()},",
            f"        .tempo = {s['tempo']},",
            f"        .speed = {s['speed']},",
            f"        .gain = {s['gain']},",
            "        .role = {",
        ]
        for r in s["roles"]:
            lines.append(f"            MUSIC_ROLE_{r.upper()},")
        lines.append("        },")
        lines.append("        .voice = {")
        for v in VOICES:
            choices = s["voices"][v]
            second = c_string(choices[1]) if len(choices) > 1 else "NULL"
            lines.append(f"            [MUSIC_VOICE_{v.upper()}] = {{ {c_string(choices[0])}, {second} }},")
        lines.append("        },")
        lines.append("        .range = {")
        for r in RANGES:
            lo, hi = s["range"][r]
            lines.append(f"            [MUSIC_RANGE_{r.upper()}] = {{ {lo}, {hi} }},")
        lines.append("        },")
        lines += [
            f"        .echo_rows = {s['echo_rows']},",
            f"        .echo_drop = {s['echo_drop']},",
            f"        .glide = {s['glide']},",
            "        .pool = {",
        ]
        for pool, entries in s["pools"].items():
            if entries:
                name = f"{s['name'].upper()}_{POOLS[pool][0]}"
                lines.append(f"            [MUSIC_POOL_{POOLS[pool][0]}] = {{ {name}, {len(entries)} }},")
        lines += ["        },", "    },"]
    lines += [
        "};",
        "const size_t MUSIC_SCORE_COUNT = sizeof(MUSIC_SCORES) / sizeof(MUSIC_SCORES[0]);",
        "",
    ]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("score", nargs="?", default=os.path.join(ROOT, "source", "musicscore.txt"))
    ap.add_argument("out", nargs="?", default=os.path.join(ROOT, "source", "musicscore.c"))
    args = ap.parse_args()
    try:
        made = generate(check(parse(open(args.score).read())))
    except ScoreError as e:
        print(f"{args.score}: {e}", file=sys.stderr)
        return 2
    if args.check:
        have = open(args.out).read() if os.path.exists(args.out) else None
        if have != made:
            print(f"{args.out} is not what {args.score} makes: run tools/music-score.py", file=sys.stderr)
            return 1
        print(f"{args.out} is what {args.score} makes")
        return 0
    open(args.out, "w").write(made)
    print(f"{args.out}: {len(made.splitlines())} lines from {args.score}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
