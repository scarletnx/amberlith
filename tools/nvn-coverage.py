#!/usr/bin/env python3
# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""Which NVN functions the probe's cases call, of the ones ScarletNX answers.

    tools/nvn-coverage.py [--check] [--gaps GROUP] [--root DIR]

Reads three things and writes docs/NvnCoverage.md from them:

- tools/nvn/nvn.hook, ScarletNX's src/nx/gpu/nvn/nvn.hook at the commit tools/nvn/PIN names: every function its NVN
  answers, each a `hook` or a `route` line under a `family`;
- nvnlayer/include/nvnlayer_procs.h, the functions the layer has, by API version;
- the cases: the C files of BUILDS, followed from each suite's entry, a call of an NVN function counted for the
  case whose ReportStart opened before it (docs/NvnCoverage.md, "How the cases are read", has the rules).

--check writes nothing: exit 1 when docs/NvnCoverage.md is not what this tree gives.
--gaps GROUP prints, as a Markdown list, the functions of GROUP no case calls (an issue's list); a GROUP that is
not one exits 2 with the groups there are.

Anything it cannot place stops it with the file and the line: a hook line of a kind it does not know, a function no
group takes, a name the layer has and the hook has not, a file that calls NVN and no build lists, a case name it
cannot follow to a string.
"""

import argparse
import hashlib
import importlib.util
import os
import re
import sys
import textwrap

HERE = os.path.dirname(os.path.abspath(__file__))
HOOK = "tools/nvn/nvn.hook"
PIN = "tools/nvn/PIN"
PROCS = "nvnlayer/include/nvnlayer_procs.h"
LAYER_HEADER = "nvnlayer/include/nvnlayer.h"
DOC = "docs/NvnCoverage.md"

# The layer's lists and the API versions whose build has each.
LISTS = {"NVN_LAYER_PROCS_BASE": (55, 231), "NVN_LAYER_PROCS_TEXTURE": (55, 231),
         "NVN_LAYER_PROCS_POOL": (55, 231), "NVN_LAYER_PROCS_SYNC": (55, 231),
         "NVN_LAYER_PROCS_SAMPLER": (55, 231), "NVN_LAYER_PROCS_BUFFER": (55, 231),
         "NVN_LAYER_PROCS_WINDOW": (55, 231), "NVN_LAYER_PROCS_WINDOW_231": (231,),
         "NVN_LAYER_PROCS_COUNTER": (55, 231), "NVN_LAYER_PROCS_RECORDING": (55, 231),
         "NVN_LAYER_PROCS_CLEAR": (55, 231), "NVN_LAYER_PROCS_STATE": (55, 231),
         "NVN_LAYER_PROCS_DRAW": (55, 231), "NVN_LAYER_PROCS_231": (231,)}
# What the layer carries outside its lists (nvnlayer_procs.h says why), in every build.
BY_HAND = ("nvnBootstrapLoader", "nvnDeviceGetInteger", "nvnDeviceGetProcAddress")

# The programs the cases are built into: the files a suite's calls are in, and where each suite starts.
BUILDS = (
    {"name": "the NRO", "api": 55, "roots": ("RunNvn",),
     "files": ("source/tests/t_nvn.c", "source/tests/t_nvnscene.c", "source/drawpath.c",
               "source/drawpath_nvn.c", "source/tests/t_nvncmd.c")},
    {"name": "the Switch 2 program", "api": 231, "roots": ("RunNvn", "RunNvn2"),
     "files": ("source/tests/t_nvn.c", "ounce/t_nvn2.c")},
)
# Where a file that calls NVN may be without a build listing it: the layer itself.
SCANNED = ("source", "ounce")

# A function entered only by a call whose chain passed this constant: drawpath.h's scene goes down three ways
# and one of them is the NVN the program looks up; the other two (deko3d's calls, the layer's own bodies) are no
# runtime's NVN.
GUARDS = {"DrawPathOpenNvn": "DRAW_PATH_NVN"}
# The same for a function that records NVN calls of its own on a way's command buffer (drawpath_nvn.h), which is
# told by its name's end: such a scene goes down the layer's own way too.
GUARDED_END = ("Recorded", "DRAW_PATH_NVN")

# What the layer's own loader calls for a program: a case that calls the loader has called these.
LOADERS = {"NvnLoadProcs": ("nvnBootstrapLoader", "nvnDeviceGetProcAddress")}

# The groups, in the order a name is tried against them: (group, families or None for any, pattern or None).
# The first that takes a name has it; one no rule takes stops the tool.
GROUPS = (
    ("dlss", ("dlss",), None),
    ("debug", None, r"Debug|Checkpoint"),
    ("ray tracing", None, r"AccelerationStructure|RayTracing|ShaderGroupHandle|RecursionDepth"),
    ("sync and events", None, r"Sync|Event"),
    ("counters and queries", None, r"Counter|ReportValue|RenderEnable|GetTimestampInNanoseconds"),
    ("windows and present", ("window",), None),
    ("windows and present", ("queue",), r"AcquireTexture|PresentTexture"),
    ("device", ("device",), None),
    ("memory pools", ("memory_pool",), None),
    ("buffers", ("buffer",), None),
    ("textures", ("texture",), None),
    ("samplers", ("sampler",), None),
    ("programs", ("program",), None),
    ("queues", ("queue",), None),
    ("state objects", ("state",), None),
    ("commands: memory and recording", ("command_buffer",),
     r"CommandBuffer(Initialize|Finalize|Add\w+Memory|BeginRecording|EndRecording|IsRecording|Get\w+Memory\w*"
     r"|\w*MemoryCallback\w*|CallCommands|CopyCommands|SetRecordingMode|Barrier|SetComputeOverlapMode"
     r"|SetComputeWorkloadPriority)$"),
    ("commands: clears and copies", ("command_buffer",),
     r"CommandBuffer(Clear|Copy|Downsample|TiledDownsample|Discard|ResolveDepthBuffer|SetCopy|GetCopy"
     r"|SaveZCullData|RestoreZCullData)"),
    ("commands: draws and dispatches", ("command_buffer",),
     r"CommandBuffer(Draw|MultiDraw|Dispatch|\w+TransformFeedback$|SetGeometryProcessingMode)"),
    ("commands: binds", ("command_buffer",),
     r"CommandBuffer(Bind|SetTexturePool|SetSamplerPool|SetProgramSubroutines|UpdateUniformBuffer|Push\w+Constants"
     r"|SetShaderScratchMemory|ShadingRateImagePalette)"),
    ("commands: render state", ("command_buffer",), r"CommandBuffer(Set|Override)"),
)
TITLES = {
    "debug": "Debug",
    "dlss": "DLSS",
    "ray tracing": "Ray tracing",
    "sync and events": "Sync and events",
    "counters and queries": "Counters and queries",
    "windows and present": "Windows and present",
    "device": "Device",
    "memory pools": "Memory pools",
    "buffers": "Buffers",
    "textures": "Textures",
    "samplers": "Samplers",
    "programs": "Programs",
    "queues": "Queues",
    "state objects": "State objects",
    "commands: memory and recording": "Commands: memory and recording",
    "commands: clears and copies": "Commands: clears and copies",
    "commands: draws and dispatches": "Commands: draws and dispatches",
    "commands: binds": "Commands: binds",
    "commands: render state": "Commands: render state",
}
# The order the document gives the groups in: the objects, then what a command buffer records, then the rest.
ORDER = ("device", "memory pools", "buffers", "textures", "samplers", "programs", "state objects", "queues",
         "sync and events", "windows and present", "counters and queries", "commands: memory and recording",
         "commands: binds", "commands: draws and dispatches", "commands: clears and copies",
         "commands: render state", "dlss", "ray tracing", "debug")

OUTSIDE = None   # the case of a call no case's ReportStart is open for


class Refused(Exception):
    """Something the tool cannot place; its text says what and where."""


# ---- the hook and the layer's lists

def read_hook(text, where=HOOK):
    """[(name, family)] of the hook and route lines, in the file's order; a route has the family "route"."""
    out, family, seen = [], None, {}
    for number, line in enumerate(text.split("\n"), 1):
        words = line.split()
        if not words or words[0].startswith("#") or words[0] == "include":
            continue
        if words[0] == "family" and len(words) == 2:
            family = words[1]
            continue
        named = re.match(r"^(hook|route)\s+(nvn\w+)\s*(\(|$)", line)
        if not named:
            raise Refused("%s:%d: neither a hook, a route, a family, an include nor a comment: %r"
                          % (where, number, line))
        kind, name = named.group(1), named.group(2)
        if kind == "hook" and family is None:
            raise Refused("%s:%d: %s comes before any family line" % (where, number, name))
        if name in seen:
            raise Refused("%s:%d: %s is also on line %d" % (where, number, name, seen[name]))
        seen[name] = number
        out.append((name, "route" if kind == "route" else family))
    return out


def group_of(name, family):
    """The group that takes @p name of @p family; a route is the device's."""
    if family == "route":
        return "device"
    for group, families, pattern in GROUPS:
        if families is not None and family not in families:
            continue
        if pattern is None or re.search(pattern, name):
            return group
    raise Refused("%s (family %s): no group of GROUPS takes it" % (name, family))


def read_lists(text, where=PROCS):
    """{name: (API versions whose build has it)} of the X-macro lists."""
    out = {}
    blocks = re.split(r"^#define\s+(NVN_LAYER_PROCS\w*)\(X\)", text, flags=re.M)
    for i in range(1, len(blocks), 2):
        macro, body = blocks[i], blocks[i + 1]
        # the list ends at the first line that does not go on
        lines = []
        for line in body.split("\n"):
            lines.append(line)
            if not line.rstrip().endswith("\\"):
                break
        names = re.findall(r"\bX\(\s*[^,]+,\s*(nvn\w+)\s*,", "\n".join(lines))
        if macro == "NVN_LAYER_PROCS":
            continue   # the lists put together
        if macro not in LISTS:
            raise Refused("%s: the list %s is not one of %s" % (where, macro, ", ".join(sorted(LISTS))))
        for name in names:
            if name in out:
                raise Refused("%s: %s is in two lists" % (where, name))
            out[name] = LISTS[macro]
    if not out:
        raise Refused("%s: no X(result, name, parameters) entry in any of %s" % (where, ", ".join(sorted(LISTS))))
    return out


# ---- the cases

TOKEN = re.compile(r"""
    (?P<space>[ \t\r\f\v]+|\\\n)
  | (?P<newline>\n)
  | (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:\\.|[^"\\\n])*")
  | (?P<char>'(?:\\.|[^'\\\n])+')
  | (?P<word>[A-Za-z_]\w*)
  | (?P<number>\.?\d(?:[\w.]|[eEpP][+-])*)
  | (?P<punct>.)
""", re.S | re.X)

API_TEST = re.compile(r"^#\s*if\s+NVN_LAYER_API\s*(<|>=)\s*(\d+)\s*$")


class Token:
    __slots__ = ("kind", "text", "line")

    def __init__(self, kind, text, line):
        self.kind, self.text, self.line = kind, text, line

    def __repr__(self):
        return "%s:%d" % (self.text, self.line)


def tokens_of(text, api, where):
    """The C tokens a build for @p api compiles: no comment, adjacent strings as one, and of the preprocessor only
    the branches of `#if NVN_LAYER_API < N` and `>= N` chosen. Any other test of NVN_LAYER_API is refused; a
    condition on something else keeps both of its branches."""
    out, line, start_of_line = [], 1, True
    stack = []   # of True (kept), False (dropped) or None (a condition on something else: kept)
    pos = 0
    while pos < len(text):
        found = TOKEN.match(text, pos)
        kind, piece = found.lastgroup, found.group()
        at = line
        if kind == "punct" and piece == "#" and start_of_line:
            # a directive, to the end of its last continued line
            end = pos
            while True:
                stop = text.find("\n", end)
                stop = len(text) if stop < 0 else stop
                if stop > 0 and text[stop - 1] == "\\":
                    end = stop + 1
                    continue
                break
            directive = re.sub(r"\s*(//.*|/\*.*?\*/)\s*$", "", text[pos:stop].replace("\\\n", " ")).strip()
            line += text.count("\n", pos, stop)
            pos = stop
            word = re.match(r"#\s*(\w+)", directive)
            word = word.group(1) if word else ""
            if word in ("if", "ifdef", "ifndef"):
                test = API_TEST.match(directive)
                if test:
                    number = int(test.group(2))
                    stack.append(api < number if test.group(1) == "<" else api >= number)
                elif "NVN_LAYER_API" in directive and word == "if":
                    raise Refused("%s:%d: a test of NVN_LAYER_API this tool does not read: %s (it reads "
                                  "`#if NVN_LAYER_API < N` and `>= N`)" % (where, at, directive))
                else:
                    stack.append(None)
            elif word == "elif":
                if not stack:
                    raise Refused("%s:%d: #elif with no #if" % (where, at))
                if stack[-1] is not None or "NVN_LAYER_API" in directive:
                    raise Refused("%s:%d: an #elif in a test of NVN_LAYER_API, which this tool does not read"
                                  % (where, at))
            elif word == "else":
                if not stack:
                    raise Refused("%s:%d: #else with no #if" % (where, at))
                if stack[-1] is not None:
                    stack[-1] = not stack[-1]
            elif word == "endif":
                if not stack:
                    raise Refused("%s:%d: #endif with no #if" % (where, at))
                stack.pop()
            continue
        pos = found.end()
        line += piece.count("\n")
        if kind == "newline":
            start_of_line = True
            continue
        if kind in ("space", "comment"):
            continue
        start_of_line = False
        if False in stack:
            continue
        if kind == "string":
            body = piece[1:-1]
            if out and out[-1].kind == "string":
                out[-1].text += body
            else:
                out.append(Token("string", body, at))
        else:
            out.append(Token(kind, piece, at))
    if stack:
        raise Refused("%s: %d #if left open at the end of the file" % (where, len(stack)))
    return out


def closing(tokens, at):
    """The index of what closes the bracket at @p at."""
    pairs = {"(": ")", "{": "}", "[": "]"}
    opener = tokens[at].text
    depth = 0
    for i in range(at, len(tokens)):
        token = tokens[i]
        if token.kind != "punct":
            continue
        if token.text == opener:
            depth += 1
        elif token.text == pairs[opener]:
            depth -= 1
            if depth == 0:
                return i
    raise Refused("line %d: a %s that nothing closes" % (tokens[at].line, opener))


def split_commas(tokens):
    """@p tokens cut at the commas outside every bracket."""
    out, depth, piece = [], 0, []
    for token in tokens:
        if token.kind == "punct" and token.text in "([{":
            depth += 1
        elif token.kind == "punct" and token.text in ")]}":
            depth -= 1
        if token.kind == "punct" and token.text == "," and depth == 0:
            out.append(piece)
            piece = []
        else:
            piece.append(token)
    if piece or out:
        out.append(piece)
    return out


CONSTANT = re.compile(r"^[A-Z][A-Z0-9_]+$")


class Constant:
    """A constant's name, as a call passed it."""

    def __init__(self, name):
        self.name = name

    def __str__(self):
        return "=" + self.name


def statement_end(tokens, at):
    """The index after the statement that starts at @p at."""
    if at >= len(tokens):
        return at
    token = tokens[at]
    if token.text == "{":
        return closing(tokens, at) + 1
    if token.kind == "word" and token.text in ("if", "for", "while", "switch") and tokens[at + 1].text == "(":
        end = statement_end(tokens, closing(tokens, at + 1) + 1)
        if token.text == "if" and end < len(tokens) and tokens[end].text == "else":
            return statement_end(tokens, end + 1)
        return end
    i = at
    while i < len(tokens) and tokens[i].text != ";":
        i = closing(tokens, i) if tokens[i].text in ("(", "{", "[") else i
        i += 1
    return i + 1


class Function:
    def __init__(self, name, where, params, body, static):
        self.name, self.where, self.params, self.body, self.static = name, where, params, body, static
        self.strings = {}   # the string constants its body declares


class Sources:
    """The functions, the string constants and the tables of functions of a build's files."""

    def __init__(self):
        self.functions = {}  # (file, name): Function
        self.shared = {}     # name: Function, of the ones that are not static
        self.strings = {}    # (file, name): text, of the constants at file scope
        self.tables = {}     # (file, name): [Function], of the initializers at file scope that name functions
        self.raw_tables = []
        self.suites = {}     # file: its SUITE

    def add(self, where, tokens):
        raw_tables = []
        i = 0
        while i < len(tokens):
            token = tokens[i]
            if token.kind == "punct" and token.text == "(" and i > 0 and tokens[i - 1].kind == "word":
                end = closing(tokens, i)
                if end + 1 < len(tokens) and tokens[end + 1].text == "{":
                    last = closing(tokens, end + 1)
                    name = tokens[i - 1].text
                    params = []
                    for piece in split_commas(tokens[i + 1:end]):
                        words = [t.text for t in piece if t.kind == "word"]
                        # a pointer to a function names itself inside its first bracket; nothing here has one
                        params.append(words[-1] if words else "")
                    back, static = i - 2, False
                    while back >= 0 and tokens[back].text not in (";", "}"):
                        static = static or tokens[back].text == "static"
                        back -= 1
                    function = Function(name, where, params, tokens[end + 2:last], static)
                    function.strings = string_constants(function.body)
                    if not static:
                        if name in self.shared:
                            raise Refused("%s:%d: %s is also in %s, and neither is static: the tool follows calls "
                                          "by name" % (where, token.line, name, self.shared[name].where))
                        self.shared[name] = function
                    self.functions[(where, name)] = function
                    i = last + 1
                    continue
                i = end + 1
                continue
            if token.kind == "punct" and token.text == "{":
                end = closing(tokens, i)
                if tokens[i - 1].text == "=":
                    back = i - 2
                    if tokens[back].text == "]":
                        while tokens[back].text != "[":
                            back -= 1
                        back -= 1
                    if tokens[back].kind == "word":
                        raw_tables.append((where, tokens[back].text,
                                           [t.text for t in tokens[i:end] if t.kind == "word"]))
                i = end + 1
                continue
            i += 1
        for name, text in string_constants(top_level(tokens)).items():
            self.strings[(where, name)] = text
            if name == "SUITE":
                self.suites[where] = text
        self.raw_tables += raw_tables

    def find(self, where, name):
        """The function a call of @p name in the file @p where reaches: that file's, or the one every file sees."""
        return self.functions.get((where, name)) or self.shared.get(name)

    def finish(self):
        for where, name, words in self.raw_tables:
            named = [self.find(where, word) for word in words]
            named = [function for function in named if function is not None]
            if named:
                self.tables[(where, name)] = named


def top_level(tokens):
    """@p tokens outside every brace."""
    out, depth = [], 0
    for token in tokens:
        if token.kind == "punct" and token.text == "{":
            depth += 1
        elif token.kind == "punct" and token.text == "}":
            depth -= 1
        elif depth == 0:
            out.append(token)
    return out


def string_constants(tokens):
    """{name: text} of each `char ... NAME = "text";` among @p tokens."""
    out = {}
    for i in range(1, len(tokens) - 2):
        if (tokens[i].text == "=" and tokens[i].kind == "punct" and tokens[i - 1].kind == "word"
                and tokens[i + 1].kind == "string" and tokens[i + 2].text == ";"):
            back = i - 2
            declared = False
            while back >= 0 and tokens[back].text not in (";", "{", "}"):
                declared = declared or tokens[back].text == "char"
                back -= 1
            if declared:
                out[tokens[i - 1].text] = tokens[i + 1].text
    return out


class Walk:
    """Follows a build's calls from its suites' entries and keeps, for each NVN function, the cases it was called
    in."""

    def __init__(self, sources):
        self.sources = sources
        self.calls = {}    # NVN function: set of (suite, case name) or OUTSIDE
        self.cases = []    # (suite, case name) in the order a ReportStart first opened each
        self.done = set()

    def run(self, root):
        function = self.sources.shared.get(root)
        if function is None:
            raise Refused("%s, a suite's entry, is in none of the build's files" % root)
        self.function(function, OUTSIDE, {}, frozenset())

    def function(self, function, case, bound, tags):
        key = (function.where, function.name, case, tuple(sorted((k, str(v)) for k, v in bound.items())), tags)
        if key in self.done:
            return
        self.done.add(key)
        self.block(function, function.body, case, bound, tags)

    def text_of(self, function, token, bound):
        """The string @p token stands for in @p function, or None."""
        if token.kind == "string":
            return token.text
        if token.kind != "word":
            return None
        if token.text in function.strings:
            return function.strings[token.text]
        if (function.where, token.text) in self.sources.strings:
            return self.sources.strings[(function.where, token.text)]
        text = bound.get(token.text)
        return None if isinstance(text, Constant) else text

    def holds(self, condition, bound):
        """Whether @p condition holds, when it is only parameters compared with constants and the call gave each
        of them one; None when it is anything else, and both ways are then read."""
        any_of = [[]]   # the tests between each two ||, every one of which has to hold
        i = 0
        while i < len(condition):
            token = condition[i]
            pair = "".join(t.text for t in condition[i + 1:i + 3])
            if token.kind == "word" and pair in ("==", "!=") and i + 3 < len(condition):
                given, other = bound.get(token.text), condition[i + 3]
                if not isinstance(given, Constant) or not CONSTANT.match(other.text):
                    return None
                any_of[-1].append((given.name == other.text) == (pair == "=="))
                i += 4
            elif "".join(t.text for t in condition[i:i + 2]) == "||" and any_of[-1]:
                any_of.append([])
                i += 2
            elif "".join(t.text for t in condition[i:i + 2]) == "&&" and any_of[-1]:
                i += 2
            else:
                return None
        if not any_of[-1]:
            return None
        return any(all(tests) for tests in any_of)

    def named(self, function, tokens, at, bound):
        """The case `Call(SUITE, NAME` at @p at names: (suite, name), with the index of the call's last token."""
        end = closing(tokens, at + 1)
        args = split_commas(tokens[at + 2:end])
        if len(args) < 2 or len(args[1]) != 1:
            raise Refused("%s:%d: %s's second argument is not one name or one string"
                          % (function.where, tokens[at].line, tokens[at].text))
        suite = self.text_of(function, args[0][0], bound) if len(args[0]) == 1 else None
        name = self.text_of(function, args[1][0], bound)
        if suite is None or name is None:
            raise Refused("%s:%d: %s(%s, %s): the tool cannot follow %s to a string, in %s"
                          % (function.where, tokens[at].line, tokens[at].text, args[0][0].text, args[1][0].text,
                             "the suite" if suite is None else "the case's name", function.name))
        return (suite, name), end

    def open_case(self, case):
        if case not in self.cases:
            self.cases.append(case)

    def block(self, function, tokens, case, bound, tags):
        """One brace's tokens, in order. @p case is the case open as the block starts; what the block opens ends
        with it."""
        i = 0
        while i < len(tokens):
            token = tokens[i]
            nxt = tokens[i + 1].text if i + 1 < len(tokens) else ""
            if token.kind == "punct" and token.text == "{":
                end = closing(tokens, i)
                self.block(function, tokens[i + 1:end], case, bound, tags)
                i = end + 1
                continue
            if token.kind == "word" and token.text == "if" and nxt == "(":
                end = closing(tokens, i + 1)
                condition = tokens[i + 2:end]
                starts = [k for k, t in enumerate(condition) if t.text == "ReportStart" and t.kind == "word"]
                if starts:
                    k = starts[0]
                    opened, _ = self.named(function, condition, k, bound)
                    self.open_case(opened)
                    negated = k > 0 and condition[k - 1].text == "!"
                    # what the statement runs when the test holds: a brace, or one statement
                    last = closing(tokens, end + 1) if tokens[end + 1].text == "{" else \
                        statement_end(tokens, end + 1) - 1
                    body = tokens[end + 1:last + 1]
                    if negated:
                        # the way out when the case is not to run; the case is open from here on
                        self.block(function, body, case, bound, tags)
                        case = opened
                    else:
                        self.block(function, body, opened, bound, tags)
                    i = last + 1
                    continue
                known = self.holds(condition, bound)
                if known is not None:
                    # a choice the call made: only the way it takes is the case's
                    then_end = statement_end(tokens, end + 1)
                    otherwise = tokens[then_end].text == "else" if then_end < len(tokens) else False
                    if known:
                        self.block(function, tokens[end + 1:then_end], case, bound, tags)
                        i = statement_end(tokens, then_end + 1) if otherwise else then_end
                    else:
                        i = then_end + 1 if otherwise else then_end
                    continue
                i += 1   # the condition and the bodies are read as they come
                continue
            if token.kind == "word" and nxt == "(":
                name = token.text
                if name == "ReportStart":
                    opened, end = self.named(function, tokens, i, bound)
                    self.open_case(opened)
                    case = opened
                    i = end + 1
                    continue
                if name == "ReportCase" or name.startswith("CHECK_"):
                    # the case's verdict: what this block does after it is the case's no more
                    judged, end = self.named(function, tokens, i, bound)
                    self.block(function, tokens[i + 2:end], case, bound, tags)
                    if judged == case:
                        case = OUTSIDE
                    i = end + 1
                    continue
                for loaded in LOADERS.get(name, ()):
                    self.calls.setdefault(loaded, set()).add(case)
                if re.match(r"^nvn[A-Z]", name):
                    self.calls.setdefault(name, set()).add(case)
                elif self.sources.find(function.where, name) is not None:
                    self.call(function, self.sources.find(function.where, name), tokens, i, case, bound, tags)
            elif token.kind == "word" and (function.where, token.text) in self.sources.tables:
                # a table of functions handed on: whoever takes it may call each
                for listed in self.sources.tables[(function.where, token.text)]:
                    self.function(listed, case, {}, tags)
            i += 1

    def call(self, function, callee, tokens, at, case, bound, tags):
        end = closing(tokens, at + 1)
        passed, constants = {}, set(tags)
        for position, piece in enumerate(split_commas(tokens[at + 2:end])):
            if len(piece) != 1:
                continue
            text = self.text_of(function, piece[0], bound)
            takes = callee.params[position] if position < len(callee.params) else ""
            given = bound.get(piece[0].text) if piece[0].kind == "word" else None
            if text is not None and takes:
                passed[takes] = text
            elif isinstance(given, Constant):
                passed[takes] = given
                constants.add(given.name)
            elif piece[0].kind == "word" and CONSTANT.match(piece[0].text):
                constants.add(piece[0].text)
                if takes:
                    passed[takes] = Constant(piece[0].text)
        guard = GUARDS.get(callee.name, GUARDED_END[1] if callee.name.endswith(GUARDED_END[0]) else None)
        if guard is not None and guard not in constants:
            return
        self.function(callee, case, passed, frozenset(constants))


def read_build(root, build):
    sources = Sources()
    for where in build["files"]:
        with open(os.path.join(root, where), encoding="utf-8") as file:
            sources.add(where, tokens_of(file.read(), build["api"], where))
    sources.finish()
    for guarded, constant in GUARDS.items():
        if guarded in sources.shared:
            if not any(constant == t.text for f in sources.functions.values() for t in f.body):
                raise Refused("%s is entered only where %s was passed, and no file of %s names %s any more"
                              % (guarded, constant, build["name"], constant))
    walk = Walk(sources)
    for entry in build["roots"]:
        walk.run(entry)
    return walk


def unlisted(root):
    """The C files under SCANNED that call an NVN function and no build lists."""
    listed = {where for build in BUILDS for where in build["files"]}
    out = []
    for folder in SCANNED:
        for path, _, names in sorted(os.walk(os.path.join(root, folder))):
            for name in sorted(names):
                where = os.path.relpath(os.path.join(path, name), root).replace(os.sep, "/")
                if not name.endswith(".c") or where in listed:
                    continue
                with open(os.path.join(root, where), encoding="utf-8") as file:
                    tokens = tokens_of(file.read(), 55, where)
                for i, token in enumerate(tokens[:-1]):
                    if token.kind == "word" and re.match(r"^nvn[A-Z]", token.text) and tokens[i + 1].text == "(":
                        out.append("%s:%d calls %s" % (where, token.line, token.text))
                        break
    return out


# ---- the table

class Table:
    def __init__(self, root):
        self.root = root
        with open(os.path.join(root, PIN), encoding="ascii") as file:
            self.pin = dict(line.split() for line in file if line.strip() and not line.startswith("#"))
        with open(os.path.join(root, HOOK), "rb") as file:
            raw = file.read()
        digest = hashlib.sha256(raw).hexdigest()
        if digest != self.pin.get("sha256"):
            raise Refused("%s has the SHA-256 %s, and %s says %s" % (HOOK, digest, PIN, self.pin.get("sha256")))
        self.hook = read_hook(raw.decode("utf-8"))
        self.group = {name: group_of(name, family) for name, family in self.hook}
        with open(os.path.join(root, PROCS), encoding="utf-8") as file:
            self.layer = read_lists(file.read())
        with open(os.path.join(root, LAYER_HEADER), encoding="utf-8") as file:
            header = file.read()
        for name in BY_HAND:
            if not re.search(r"\b%s\b" % name, header):
                raise Refused("%s no longer names %s, which the layer carried outside its lists"
                              % (LAYER_HEADER, name))
            self.layer[name] = (55, 231)
        missing = sorted(name for name in self.layer if name not in self.group)
        if missing:
            raise Refused("the layer has %s, which %s at %s has not"
                          % (", ".join(missing), HOOK, self.pin["commit"][:12]))
        stray = unlisted(root)
        if stray:
            raise Refused("a file that calls NVN and no build of BUILDS lists: " + "; ".join(stray))
        self.cases = []      # (suite, name)
        self.where = {}      # (suite, name): [build names]
        self.calls = {}      # function: set of cases, OUTSIDE among them
        for build in BUILDS:
            walk = read_build(root, build)
            for case in walk.cases:
                if case not in self.cases:
                    self.cases.append(case)
                self.where.setdefault(case, []).append(build["name"])
            for name, cases in walk.calls.items():
                if name not in self.group:
                    raise Refused("%s calls %s, which %s at %s has not"
                                  % (build["name"], name, HOOK, self.pin["commit"][:12]))
                self.calls.setdefault(name, set()).update(cases)
        # a case's label: its suite and its place in the suite
        self.label, counts = {}, {}
        for suite, name in self.cases:
            counts[suite] = counts.get(suite, 0) + 1
            self.label[(suite, name)] = "%s-%d" % (suite, counts[suite])

    def in_a_case(self, name):
        return any(case is not OUTSIDE for case in self.calls.get(name, ()))

    def names(self, group):
        return [name for name, _ in self.hook if self.group[name] == group]

    def gaps(self, group):
        return [name for name in self.names(group) if not self.in_a_case(name)]

    def cell(self, name):
        cases = self.calls.get(name, set())
        labels = [self.label[case] for case in self.cases if case in cases]
        if labels:
            return ", ".join(labels)
        return "outside a case" if OUTSIDE in cases else ""

    def document(self):
        total = len(self.hook)
        layer55 = sum(1 for name in self.layer if 55 in self.layer[name])
        layer231 = len(self.layer)
        called = sum(1 for name, _ in self.hook if self.in_a_case(name))
        outside = sum(1 for name, _ in self.hook if not self.in_a_case(name) and name in self.calls)
        out = []
        add = out.append
        add("# NVN coverage")
        add("")
        add("<!-- Written by tools/nvn-coverage.py: change the cases, the layer or the tool, and run it. -->")
        add("")
        intro = ("Which of the NVN functions ScarletNX answers a case of the probe calls. ScarletNX's `nvn.hook` at "
                 "%s names %d; the layer has %d of them in its API 55 build and %d in its API 231 build; a case "
                 "calls %d%s. The other %d have no call at all: a runtime could answer any of them wrongly and "
                 "every case would still pass."
                 % (self.pin["commit"][:12], total, layer55, layer231, called,
                    "" if outside == 0 else ", and %d more are called only outside a case, as a suite puts its "
                    "objects away" % outside, total - called - outside))
        out.extend(textwrap.wrap(intro, 110))
        add("")
        add("## How to read it")
        add("")
        add("- **A function** is a `hook` or a `route` line of `tools/nvn/nvn.hook`, the copy of ScarletNX's")
        add("  `src/nx/gpu/nvn/nvn.hook` that `tools/nvn/PIN` names by commit and SHA-256.")
        add("- **Its group** is the hook's own `family`, with the command buffer's functions told apart by what")
        add("  they record and four groups taken by name from every family but DLSS's: debug, ray tracing,")
        add("  sync and events, counters and queries. `GROUPS` in the tool is the rule, tried in its order.")
        add("- **Layer** is the API versions whose build of [the layer](NvnLayer.md) has the function: a case")
        add("  can call only what the layer has.")
        add("- **Cases** are the cases whose run calls the function, by the labels of [The cases](#the-cases).")
        add("  Called says nothing of judged: a builder's setter is called by every case that makes the object,")
        add("  and only a case that reads the result back holds the runtime to it.")
        add("")
        add("## How the cases are read")
        add("")
        add("From the C files, with no compiler and no run, so the table is the same wherever it is made:")
        add("")
        for build in BUILDS:
            add("- **%s** (API %d): %s, from %s." % (
                build["name"], build["api"], ", ".join("`%s`" % where for where in build["files"]),
                " and ".join("`%s`" % entry for entry in build["roots"])))
        add("")
        add("A call belongs to the case whose `ReportStart` opened before it: to the end of the brace of an")
        add("`if (ReportStart(...))`, or, after an `if (!ReportStart(...))` that leaves, until the case's own")
        add("`CHECK_` or `ReportCase` in that brace. A function called inside a case is the case's too, with")
        add("the functions it calls, and a table of functions counts for whoever names the table. Where a")
        add("function's `if` only compares its parameters with constants a call passed, the way that call takes")
        add("is the one read; any other choice counts both ways. A call of the layer's `NvnLoadProcs` is a call")
        add("of the two functions it gets every other through. A call no case is open for is **outside a")
        add("case**. Of the scene's three ways (`source/drawpath.h`) only the one")
        add("through the NVN the program looks up is counted: the other two are deko3d's calls and the layer's")
        add("own bodies, which no runtime's NVN takes. `#if NVN_LAYER_API` chooses a build's lines.")
        add("")
        add("`tools/nvn-coverage.py --check` says whether this file is what the tree gives, and the host build's")
        add("tests run it; `--gaps GROUP` lists a group's functions with no case.")
        add("")
        add("## By group")
        add("")
        add("| Group | Functions | Layer, API 55 | Layer, API 231 | Called by a case | No case |")
        add("| --- | ---: | ---: | ---: | ---: | ---: |")
        sums = [0, 0, 0, 0, 0]
        for group in ORDER:
            names = self.names(group)
            row = [len(names),
                   sum(1 for name in names if 55 in self.layer.get(name, ())),
                   sum(1 for name in names if name in self.layer),
                   sum(1 for name in names if self.in_a_case(name)),
                   len(self.gaps(group))]
            sums = [a + b for a, b in zip(sums, row)]
            add("| [%s](#%s) | %d | %d | %d | %d | %d |" % ((TITLES[group], anchor(TITLES[group])) + tuple(row)))
        add("| **All** | **%d** | **%d** | **%d** | **%d** | **%d** |" % tuple(sums))
        add("")
        add("## The cases")
        add("")
        add("| Label | Suite | Case | In |")
        add("| --- | --- | --- | --- |")
        for case in self.cases:
            add("| %s | %s | %s | %s |" % (self.label[case], case[0], case[1], ", ".join(self.where[case])))
        add("")
        for group in ORDER:
            names = self.names(group)
            gaps = self.gaps(group)
            add("## %s" % TITLES[group])
            add("")
            add("%d functions, %d with no case." % (len(names), len(gaps)))
            add("")
            add("| Function | Layer | Cases |")
            add("| --- | --- | --- |")
            for name in names:
                versions = self.layer.get(name, ())
                add("| `%s` | %s | %s |" % (name, ", ".join(str(v) for v in versions), self.cell(name)))
            add("")
        return indexed("\n".join(out))


def anchor(title):
    return re.sub(r"[^\w\- ]", "", title.lower()).replace(" ", "-")


def indexed(text):
    """@p text with the index tools/doc-index.py gives it."""
    spec = importlib.util.spec_from_file_location("doc_index", os.path.join(HERE, "doc-index.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.indexed(text, 3)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 when %s is stale" % DOC)
    parser.add_argument("--gaps", metavar="GROUP", help="print the functions of GROUP no case calls")
    parser.add_argument("--root", default=os.path.dirname(HERE), help="the tree to read (this tool's)")
    args = parser.parse_args(argv)
    try:
        table = Table(args.root)
        if args.gaps is not None:
            if args.gaps not in ORDER:
                print("nvn-coverage: --gaps %s is not a group: %s" % (args.gaps, ", ".join(ORDER)), file=sys.stderr)
                return 2
            for name in table.gaps(args.gaps):
                print("- `%s`%s" % (name, "" if name in table.layer else " (not in the layer)"))
            return 0
        text = table.document()
    except Refused as refused:
        print("nvn-coverage: %s" % refused, file=sys.stderr)
        return 1
    path = os.path.join(args.root, DOC)
    old = None
    if os.path.exists(path):
        with open(path, encoding="utf-8") as file:
            old = file.read()
    if args.check:
        if old != text:
            print("nvn-coverage: %s is not what this tree gives: run tools/nvn-coverage.py" % DOC, file=sys.stderr)
            return 1
        return 0
    if old != text:
        with open(path, "w", encoding="utf-8") as file:
            file.write(text)
    print("nvn-coverage: %s: %d functions, %d called by a case" % (
        DOC, len(table.hook), sum(1 for name, _ in table.hook if table.in_a_case(name))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
