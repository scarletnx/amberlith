# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/deko3d.sh builds nothing again for a directory it built from its two commits and its own text, only renewing
the stamp make reads, and leaves the library and the headers as they were, so no object rebuilds; a stamp of another
commit or another script (which names the dekotools commit too) is not one it built. Offline: the directory is made
here, so a run that would build stops at the clone, which has no network to reach."""

import hashlib
import os
import re
import subprocess
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "deko3d.sh")


def pinned(name):
    with open(SCRIPT) as f:
        found = re.search(r"^%s=([0-9a-f]{40})$" % name, f.read(), re.M)
    return found.group(1)


def stamp():
    with open(SCRIPT, "rb") as f:
        text = f.read()
    return "%s %s" % (pinned("commit"), hashlib.sha256(text).hexdigest()[:16])


class Deko3dScript(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="amberlith-deko3d-test-")
        self.dir = os.path.join(self.tmp.name, "deko3d")
        os.makedirs(os.path.join(self.dir, "lib"))
        os.makedirs(os.path.join(self.dir, "include"))
        self.files = [os.path.join(self.dir, "lib", "libdeko3d.a"), os.path.join(self.dir, "include", "deko3d.h")]
        for path in self.files:
            with open(path, "w") as f:
                f.write("built\n")
            os.utime(path, (1, 1))
        self.stamp = os.path.join(self.dir, "deko3d.stamp")
        # No network and no prompt: a run that tries a clone fails rather than hangs.
        self.env = dict(
            os.environ,
            DEVKITPRO=self.tmp.name,
            GIT_TERMINAL_PROMPT="0",
            GIT_ASKPASS="/bin/false",
            GIT_CONFIG_GLOBAL=os.devnull,
            GIT_CONFIG_NOSYSTEM="1",
            https_proxy="http://127.0.0.1:9",
            HTTPS_PROXY="http://127.0.0.1:9",
        )

    def tearDown(self):
        self.tmp.cleanup()

    def run_script(self, *args, env=None):
        return subprocess.run(
            ["sh", SCRIPT, *args], env=env or self.env, capture_output=True, text=True, timeout=120
        )

    def test_built_from_its_commits_is_only_renewed(self):
        with open(self.stamp, "w") as f:
            f.write(stamp() + "\n")
        os.utime(self.stamp, (1, 1))
        before = time.time()
        done = self.run_script(self.dir)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertGreaterEqual(os.stat(self.stamp).st_mtime, before - 1)
        for path in self.files:
            self.assertEqual(os.stat(path).st_mtime, 1, path)
        self.assertFalse(os.path.exists(os.path.join(self.dir, "src")))

    def test_a_stamp_of_another_commit_is_built_again(self):
        with open(self.stamp, "w") as f:
            f.write(stamp().replace(pinned("commit"), "0" * 40) + "\n")
        done = self.run_script(self.dir)
        self.assertNotEqual(done.returncode, 0)
        self.assertTrue(os.path.exists(os.path.join(self.dir, "dekotools-src")) or "dekotools" in done.stderr)

    def test_a_stamp_of_another_script_is_built_again(self):
        with open(self.stamp, "w") as f:
            f.write(stamp()[:-16] + "0" * 16 + "\n")
        done = self.run_script(self.dir)
        self.assertNotEqual(done.returncode, 0)

    def test_a_wrong_count_of_arguments_is_refused(self):
        done = self.run_script()
        self.assertEqual(done.returncode, 2)
        self.assertIn("usage: tools/deko3d.sh DIR", done.stderr)

    def test_no_devkitpro_is_refused(self):
        env = {k: v for k, v in self.env.items() if k != "DEVKITPRO"}
        done = self.run_script(self.dir, env=env)
        self.assertEqual(done.returncode, 2)
        self.assertIn("DEVKITPRO is not set", done.stderr)


if __name__ == "__main__":
    unittest.main()
