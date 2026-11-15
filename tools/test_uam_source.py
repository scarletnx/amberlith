# SPDX-FileCopyrightText: Copyright 2026 edenprobe contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/uam-source.sh leaves the uam build's source a clone of the pinned repository at the pinned commit, whatever
was there before: nothing, the same clone, or a clone of the toolchain's uam made before the pin moved to the fork,
which has none of the fork's commits. Offline: two local repositories stand for devkitPro/uam and scarletnx/uam, the
pinned commit on a branch of its own as the fork's fix is."""

import os
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "uam-source.sh")

# Away from the configuration of the machine it runs on (signing, hooks, a default branch), and from any prompt.
ENV = dict(
    os.environ,
    GIT_CONFIG_GLOBAL=os.devnull,
    GIT_CONFIG_NOSYSTEM="1",
    GIT_TERMINAL_PROMPT="0",
    GIT_AUTHOR_NAME="edenprobe test",
    GIT_AUTHOR_EMAIL="test@edenprobe.invalid",
    GIT_COMMITTER_NAME="edenprobe test",
    GIT_COMMITTER_EMAIL="test@edenprobe.invalid",
)


def git(*args, cwd=None):
    return subprocess.run(
        ["git", *args], cwd=cwd, env=ENV, check=True, capture_output=True, text=True
    ).stdout.strip()


def commit(repo, name, text):
    with open(os.path.join(repo, name), "w") as f:
        f.write(text)
    git("add", name, cwd=repo)
    git("commit", "--quiet", "-m", name, cwd=repo)
    return git("rev-parse", "HEAD", cwd=repo)


class UamSource(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="edenprobe-uam-test-")
        root = self.tmp.name
        self.toolchain = os.path.join(root, "devkitpro-uam")
        git("init", "--quiet", "--initial-branch=master", self.toolchain)
        self.release = commit(self.toolchain, "uam.c", "uam 1.1.0\n")
        self.fork = os.path.join(root, "scarletnx-uam")
        git("clone", "--quiet", self.toolchain, self.fork)
        git("checkout", "--quiet", "-b", "fix", cwd=self.fork)
        self.pinned = commit(self.fork, "fix.c", "a uniform vector's component\n")
        git("checkout", "--quiet", "master", cwd=self.fork)
        self.root = root

    def tearDown(self):
        self.tmp.cleanup()

    def source(self, src):
        return subprocess.run(
            ["sh", SCRIPT, src, self.fork, self.pinned], env=ENV, capture_output=True, text=True
        )

    def assertPinned(self, src, done):
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(git("rev-parse", "HEAD", cwd=src), self.pinned)
        self.assertEqual(git("remote", "get-url", "origin", cwd=src), self.fork)

    def test_no_clone_yet(self):
        src = os.path.join(self.root, "fresh")
        self.assertPinned(src, self.source(src))

    def test_the_toolchains_clone_from_before_the_pin(self):
        src = os.path.join(self.root, "old")
        git("clone", "--quiet", self.toolchain, src)
        self.assertEqual(git("rev-parse", "HEAD", cwd=src), self.release)
        self.assertPinned(src, self.source(src))

    def test_a_clone_with_a_patch_applied_on_it(self):
        src = os.path.join(self.root, "patched")
        self.assertPinned(src, self.source(src))
        with open(os.path.join(src, "fix.c"), "a") as f:
            f.write("a patch of tools/uam\n")
        self.assertPinned(src, self.source(src))
        self.assertEqual(git("status", "--porcelain", cwd=src), "")

    def test_a_wrong_count_of_arguments_is_refused(self):
        done = subprocess.run(["sh", SCRIPT, "only-one"], env=ENV, capture_output=True, text=True)
        self.assertEqual(done.returncode, 2)
        self.assertIn("usage: tools/uam-source.sh SRC REPO COMMIT", done.stderr)


if __name__ == "__main__":
    unittest.main()
