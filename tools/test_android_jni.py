# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""The APK's native code names the Java side it binds to: every app class a FindClass asks for is a class of
android/src/main/java, and every native function's JNI name is a native method its class declares. A name either side
lost (a package renamed on one side only) builds, and then ART refuses the library or the call at run time, which no
build and no fake adb shows."""

import os
import re
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CPP = os.path.join(ROOT, "android", "src", "main", "cpp")
JAVA = os.path.join(ROOT, "android", "src", "main", "java")

FIND_CLASS = re.compile(r'FindClass\(\s*env\s*,\s*"([^"]+)"\s*\)')
JNI_NAME = re.compile(r"\bJava_((?:[a-z0-9]+_)+)([A-Z][A-Za-z0-9]*)_([A-Za-z0-9]+)\b")
PROBE_FN = re.compile(r"#define PROBE_FN\(name\) Java_((?:[a-z0-9]+_)+)([A-Z][A-Za-z0-9]*)_##name")
PROBE_USE = re.compile(r"\bPROBE_FN\(([A-Za-z0-9]+)\)\(")


def sources():
    for name in sorted(os.listdir(CPP)):
        if name.endswith((".c", ".h")):
            with open(os.path.join(CPP, name), encoding="utf-8") as file:
                yield name, file.read()


def java_class(path):
    """The source of the class at @p path (a/b/C), or None when android/src/main/java has none."""
    file = os.path.join(JAVA, path + ".java")
    if not os.path.isfile(file):
        return None
    with open(file, encoding="utf-8") as source:
        return source.read()


def bindings():
    """Every (file, class path, method) the native code binds by its JNI name."""
    found = []
    macro = None
    for name, text in sources():
        match = PROBE_FN.search(text)
        if match:
            macro = match.group(1).rstrip("_").replace("_", "/") + "/" + match.group(2)
    for name, text in sources():
        for package, cls, method in JNI_NAME.findall(text):
            found.append((name, package.rstrip("_").replace("_", "/") + "/" + cls, method))
        for method in PROBE_USE.findall(text):
            found.append((name, macro, method))
    return found


class AndroidJni(unittest.TestCase):
    def test_every_app_class_a_findclass_names_is_one(self):
        asked = [(name, path) for name, text in sources() for path in FIND_CLASS.findall(text)]
        apps = [(name, path) for name, path in asked if not path.startswith(("java/", "android/", "dalvik/"))]
        self.assertTrue(apps, "no FindClass of an app class found: the pattern no longer matches the sources")
        for name, path in apps:
            with self.subTest(file=name, cls=path):
                self.assertIsNotNone(java_class(path), '%s: FindClass("%s") names no class of %s' % (name, path, JAVA))

    def test_every_native_function_is_a_native_method_of_its_class(self):
        found = bindings()
        self.assertTrue(any(method == "nativeRun" for _, _, method in found), "the Engine's nativeRun was not found")
        self.assertTrue(any(name == "jni_cases.c" for name, _, _ in found), "PROBE_FN's uses were not found")
        for name, path, method in found:
            with self.subTest(file=name, cls=path, method=method):
                self.assertIsNotNone(path, "%s: PROBE_FN's class is not defined" % name)
                source = java_class(path)
                self.assertIsNotNone(source, "%s: %s binds %s, a class %s has not" % (name, method, path, JAVA))
                self.assertRegex(
                    source,
                    r"\bnative\b[^;(]*\b%s\s*\(" % re.escape(method),
                    "%s: %s.%s is no native method of the class" % (name, path, method),
                )


if __name__ == "__main__":
    unittest.main()
