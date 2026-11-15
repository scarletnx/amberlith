#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Turns the demos' scores, source/musicscore.txt, into the tables music.c plays, source/musicscore.c.

    tools/music-score.py [--check] [SCORE [OUT]]

The score is the music as written (see the comment at its top); the C file is its tables, generated, never
edited by hand. Every bar must fill its steps, every note must be a note, every chord a major or minor triad on a
named root, every pattern one the score defines and as long as a bar, every section in the form one the track
writes. A score that breaks one is refused with the line, what it holds and what it should.

--check writes nothing: it says whether OUT is what SCORE makes, and exits 1 when it is not.
"""

import argparse
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
MODES = ["ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian"]
LANES = ["bass", "drums", "hats", "comp", "arp"]
VOICES = ["bass", "chords", "lead", "counter", "arp"]
PITCH = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
GRID = {
    "bass": set("R3578A-."),
    "drums": set("KSs."),
    "hats": set("hHo."),
    "comp": set("X-."),
    "arp": set("123456."),
}


class ScoreError(Exception):
    pass


def pitch_class(name, where):
    m = re.fullmatch(r"([A-G])([#b]?)", name)
    if not m:
        raise ScoreError(f"{where}: {name!r} is no note name: a letter from A to G, then # or b or nothing")
    return (PITCH[m.group(1)] + {"#": 1, "b": -1, "": 0}[m.group(2)]) % 12


def note(token, where):
    """NAME OCTAVE:STEPS[~] -> (midi, steps, glide); r:STEPS -> (0, steps, 0)."""
    m = re.fullmatch(r"(r|[A-G][#b]?[0-8]):([0-9]+)(~?)", token)
    if not m:
        raise ScoreError(f"{where}: {token!r} is no note: NAME OCTAVE:STEPS (C5:4), r:STEPS, a ~ to glide")
    steps = int(m.group(2))
    if steps < 1 or steps > 255:
        raise ScoreError(f"{where}: {token!r} lasts {steps} steps, where 1 to 255")
    if m.group(1) == "r":
        if m.group(3):
            raise ScoreError(f"{where}: {token!r} glides into a rest")
        return (0, steps, 0)
    name, octave = m.group(1)[:-1], int(m.group(1)[-1])
    midi = 12 * (octave + 1) + PITCH[name[0]] + {"#": 1, "b": -1, "": 0}[name[1:]]
    if not 24 <= midi <= 108:
        raise ScoreError(f"{where}: {token!r} is MIDI note {midi}, outside C1 to C8 (24 to 108)")
    return (midi, steps, 1 if m.group(3) else 0)


def chord(symbol, where):
    m = re.fullmatch(r"([A-G][#b]?)(m?)", symbol)
    if not m:
        raise ScoreError(f"{where}: {symbol!r} is no chord: a root (C, Bb, F#) and m for minor")
    return (pitch_class(m.group(1), where), 1 if m.group(2) else 0)


def bars(body, where):
    parts = body.strip()
    if not parts.startswith("|") or not parts.endswith("|"):
        raise ScoreError(f"{where}: bars are written between | and |")
    return [b.strip() for b in parts[1:-1].split("|")]


def parse(text):
    patterns = {}
    tracks = []
    track = None
    for number, raw in enumerate(text.split("\n"), 1):
        line = raw.rstrip()
        # A comment is a whole line: a # inside one is a sharp.
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        where = f"line {number}"
        words = line.split()
        key = words[0]
        if key == "pattern":
            if len(words) != 4 or words[1] not in LANES:
                raise ScoreError(f"{where}: a pattern is `pattern LANE NAME GRID`, LANE one of {' '.join(LANES)}")
            lane, name, grid = words[1], words[2], words[3]
            wrong = set(grid) - GRID[lane]
            if wrong:
                raise ScoreError(f"{where}: {lane} pattern {name} holds {''.join(sorted(wrong))!r}, "
                                 f"where only {''.join(sorted(GRID[lane]))!r}")
            if (lane, name) in patterns:
                raise ScoreError(f"{where}: {lane} pattern {name} is written twice")
            patterns[(lane, name)] = grid
            continue
        if key == "track":
            track = {"name": words[1], "line": number, "sections": {}, "order": []}
            tracks.append(track)
            continue
        if track is None:
            raise ScoreError(f"{where}: {key!r} before any track")
        rest = line.split(None, 1)[1] if len(words) > 1 else ""
        if key in ("demo", "key"):
            track[key] = rest
        elif key in ("tempo", "steps", "swing", "gain"):
            if not re.fullmatch(r"[0-9]+", rest):
                raise ScoreError(f"{where}: {key} {rest!r} is not a whole number")
            track[key] = int(rest)
        elif key == "voices":
            track["voices"] = dict(w.split("=", 1) for w in words[1:])
        elif key == "form":
            track["form"] = words[1:]
        elif key in ("chords", "lead", "counter", "parts"):
            section = track["sections"].setdefault(words[1], {"name": words[1]})
            if words[1] not in track["order"]:
                track["order"].append(words[1])
            body = line.split(None, 2)[2] if len(words) > 2 else ""
            if key == "parts":
                section["parts"] = {w.split("=")[0]: w.split("=")[1] for w in body.split() if "=" in w}
                section["fill"] = "fill" in body.split()
            elif key == "chords":
                section["chords"] = [chord(b, f"{where}, {track['name']} {words[1]}") for b in bars(body, where)]
            else:
                section[key] = [[note(t, f"{where}, {track['name']} {words[1]} {key} bar {i + 1}") for t in b.split()]
                                for i, b in enumerate(bars(body, where))]
        else:
            raise ScoreError(f"{where}: {key!r} is no word a track takes")
    for t in tracks:
        check(t, patterns)
    return tracks, patterns


def check(t, patterns):
    name = t["name"]
    for field in ("demo", "key", "tempo", "steps", "swing", "gain", "voices", "form"):
        if field not in t:
            raise ScoreError(f"track {name}: no {field}")
    tonic, mode = t["key"].split()
    t["tonic"] = pitch_class(tonic, f"track {name} key")
    if mode not in MODES:
        raise ScoreError(f"track {name}: key {t['key']!r}, its mode none of {' '.join(MODES)}")
    t["mode"] = MODES.index(mode)
    if t["steps"] not in (16, 24):
        raise ScoreError(f"track {name}: steps {t['steps']}, where 16 (4/4) or 24 (12/8)")
    if not 40 <= t["tempo"] <= 240:
        raise ScoreError(f"track {name}: tempo {t['tempo']}, where 40 to 240")
    if t["swing"] > 32:
        raise ScoreError(f"track {name}: swing {t['swing']}, where 0 to 32 64ths of a step")
    if not 1 <= t["gain"] <= 64:
        raise ScoreError(f"track {name}: gain {t['gain']}, where 1 to 64")
    for v in VOICES:
        if v not in t["voices"]:
            raise ScoreError(f"track {name}: no {v} in voices")
    for s in t["form"]:
        if s not in t["sections"]:
            raise ScoreError(f"track {name}: the form plays {s}, which the track does not write")
    steps = t["steps"]
    for sname, s in t["sections"].items():
        where = f"track {name} section {sname}"
        if "chords" not in s or "parts" not in s:
            raise ScoreError(f"{where}: no chords or no parts")
        count = len(s["chords"])
        for part in ("lead", "counter"):
            if part not in s:
                s[part] = [[(0, steps, 0)] for _ in range(count)]
            if len(s[part]) != count:
                raise ScoreError(f"{where}: {part} has {len(s[part])} bars, the chords {count}")
            for i, bar in enumerate(s[part]):
                total = sum(n[1] for n in bar)
                if total != steps:
                    raise ScoreError(f"{where}: {part} bar {i + 1} lasts {total} steps, where {steps}")
        for lane, key in (("bass", "bass"), ("drums", "drums"), ("hats", "drums"), ("comp", "chords"), ("arp", "arp")):
            pname = s["parts"].get(key)
            if pname is None:
                raise ScoreError(f"{where}: parts names no {key}")
            names = [pname] + ([pname + "fill"] if lane == "drums" and s["fill"] else [])
            for n in names:
                grid = patterns.get((lane, n))
                if grid is None:
                    raise ScoreError(f"{where}: no {lane} pattern {n}")
                if len(grid) != steps:
                    raise ScoreError(f"{where}: {lane} pattern {n} is {len(grid)} steps, the bar {steps}")


def c_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def generate(tracks, patterns):
    out = [
        "// SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors",
        "// SPDX-License-Identifier: GPL-3.0-or-later",
        "",
        "// Generated by tools/music-score.py from source/musicscore.txt: edit the score, not this file.",
        "",
        '#include "musicscore.h"',
        "",
    ]
    out.append("const MusicScorePattern MUSIC_SCORE_PATTERNS[] = {")
    for (lane, name), grid in patterns.items():
        out.append(f"    {{ MUSIC_LANE_{lane.upper()}, {c_string(name)}, {c_string(grid)} }},")
    out.append("};")
    out.append(f"const size_t MUSIC_SCORE_PATTERN_COUNT = {len(patterns)}u;")
    out.append("")
    for t in tracks:
        tag = t["name"].upper()
        for sname in t["order"]:
            s = t["sections"][sname]
            stag = f"{tag}_{re.sub(r'[^A-Za-z0-9]', '_', sname).upper()}"
            out.append(f"static const MusicScoreChord {stag}_CHORDS[] = {{")
            out.append("    " + ", ".join(f"{{ {r}, {m} }}" for r, m in s["chords"]) + ",")
            out.append("};")
            for part in ("lead", "counter"):
                notes = [n for bar in s[part] for n in bar]
                out.append(f"static const MusicScoreNote {stag}_{part.upper()}[] = {{")
                for i in range(0, len(notes), 6):
                    out.append("    " + " ".join(f"{{ {a}, {b}, {c} }}," for a, b, c in notes[i:i + 6]))
                out.append("};")
        out.append(f"static const MusicScoreSection {tag}_SECTIONS[] = {{")
        for sname in t["order"]:
            s = t["sections"][sname]
            stag = f"{tag}_{re.sub(r'[^A-Za-z0-9]', '_', sname).upper()}"
            p = s["parts"]
            nl = sum(len(b) for b in s["lead"])
            nc = sum(len(b) for b in s["counter"])
            out.append(f"    {{ {c_string(sname)}, {len(s['chords'])}u, {stag}_CHORDS, {stag}_LEAD, {nl}u, "
                       f"{stag}_COUNTER, {nc}u,")
            out.append(f"      {{ {c_string(p['bass'])}, {c_string(p['drums'])}, {c_string(p['chords'])}, "
                       f"{c_string(p['arp'])} }}, {'true' if s['fill'] else 'false'} }},")
        out.append("};")
        form = [t["order"].index(s) for s in t["form"]]
        out.append(f"static const uint8_t {tag}_FORM[] = {{ {', '.join(str(i) for i in form)} }};")
        out.append("")
    out.append("const MusicScore MUSIC_SCORES[] = {")
    for t in tracks:
        tag = t["name"].upper()
        v = t["voices"]
        out.append(f"    {{ {c_string(t['name'])}, {c_string(t['demo'])}, {c_string(t['key'])}, {t['tonic']}u, "
                   f"{t['mode']}u, {t['tempo']}u, {t['steps']}u, {t['swing']}u, {t['gain']}u,")
        out.append(f"      {{ {', '.join(c_string(v[k]) for k in VOICES)} }},")
        out.append(f"      {tag}_FORM, {len(t['form'])}u, {tag}_SECTIONS, {len(t['order'])}u }},")
    out.append("};")
    out.append(f"const size_t MUSIC_SCORE_COUNT = {len(tracks)}u;")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    ap.add_argument("score", nargs="?", default=os.path.join(ROOT, "source", "musicscore.txt"))
    ap.add_argument("out", nargs="?", default=os.path.join(ROOT, "source", "musicscore.c"))
    args = ap.parse_args()
    try:
        text = generate(*parse(open(args.score, encoding="utf-8").read()))
    except ScoreError as e:
        print(f"music-score: {args.score}: {e}", file=sys.stderr)
        return 1
    if args.check:
        have = open(args.out, encoding="utf-8").read() if os.path.exists(args.out) else ""
        if have != text:
            print(f"music-score: {args.out} is not what {args.score} makes: run tools/music-score.py", file=sys.stderr)
            return 1
        return 0
    open(args.out, "w", encoding="utf-8").write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
