# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""nightly-notes.py on a history of its own: merged pull requests, direct commits, a branch never merged; the files'
names after the version, the NRO, the APK, the VPK, the 3DSX, the NDS and the ExeFS zips with their sizes and SHA-256, and text
that would be markdown shown as it is."""

import importlib.util
import os
import re
import tempfile
import unittest

from fake_github import World

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("nightly_notes", os.path.join(HERE, "nightly-notes.py"))
nightly = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(nightly)

WHERE = "https://github.com/owner/repo/releases/download/t"
URL = "https://github.com/owner/repo"
DIGEST = "ab" * 32
NAMES = nightly.names("0.2.0", "0123abc")
NRO = NAMES["nro"]
# a release of the NRO alone
ALONE = [nightly.Built("nro", NRO, 1_797_929, DIGEST)]


class NightlyNotes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        world = cls.world = World(cls.temporary.name, "owner/repo")
        # written on the 1st, landed on main on the 15th: the title dates the landing
        cls.previous = world.commit("the previous nightly")
        world.git("switch", "-q", "-c", "feature")
        world.commit("amberlith: a commit of the pull request", date="2026-10-01T12:00:00Z")
        world.git("switch", "-q", "main")
        world.git(
            "merge", "-q", "--no-ff", "feature", "-m", "Merge pull request #5 from someone/feature\n\n[ci] its title"
        )
        cls.merge = world.git("rev-parse", "HEAD")
        cls.sha = world.commit("amberlith: a commit rebased onto main")
        world.git("switch", "-q", "-c", "elsewhere", cls.previous)
        cls.unmerged = world.commit("never merged")
        world.git("switch", "-q", "main")
        cls.checkout = str(world.checkout)
        cls.text = nightly.notes(cls.checkout, cls.sha, cls.previous, "owner/repo", WHERE, ALONE, "Kept.")

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_changes_newest_first(self):
        self.assertEqual(
            nightly.changes(self.checkout, self.previous, self.sha),
            [("amberlith: a commit rebased onto main", None, self.sha), ("[ci] its title", 5, self.merge)],
        )

    def test_change_lines_link_the_repository(self):
        self.assertEqual(
            nightly.change_line(nightly.Change("[ci] its title", 5, self.merge), URL),
            f"\\[ci\\] its title ([#5]({URL}/pull/5))",
        )
        self.assertEqual(
            nightly.change_line(nightly.Change("a fix", None, self.sha), URL),
            f"a fix ([`{self.sha[:7]}`]({URL}/commit/{self.sha}))",
        )

    def test_markdown_shown_as_it_is(self):
        self.assertEqual(
            nightly.escape("[x](http://e) <b> a|b `c` *d* _e_ ~f~ \\"),
            "\\[x\\](http://e) \\<b\\> a\\|b \\`c\\` \\*d\\* \\_e\\_ \\~f\\~ \\\\",
        )

    def test_no_changes_without_an_ancestor(self):
        self.assertIsNone(nightly.changes(self.checkout, "", self.sha))
        self.assertIsNone(nightly.changes(self.checkout, self.unmerged, self.sha))

    def test_title_dates_the_landing(self):
        self.assertEqual(nightly.title(self.checkout, self.sha), f"Amberlith nightly 2026-10-15 ({self.sha[:7]})")

    def test_the_built_commit_is_named_first(self):
        # where the next nightly looks for the commit this one was built from
        self.assertEqual(re.search(r"[0-9a-f]{40}", self.text).group(0), self.sha)

    def test_names_after_the_version(self):
        self.assertEqual(
            nightly.names("0.18.0"),
            {
                "nro": "amberlith-0.18.0.nro",
                "apk": "amberlith-0.18.0.apk",
                "vpk": "amberlith-0.18.0.vpk",
                "3dsx": "amberlith-0.18.0.3dsx",
                "nds": "amberlith-0.18.0.nds",
                "ounce": "amberlith-ounce-0.18.0.zip",
                "mod": "amberlith-mod-0.18.0.zip",
            },
        )
        self.assertEqual(
            nightly.names("0.18.0", "0123abc"),
            {
                "nro": "amberlith-0.18.0-0123abc.nro",
                "apk": "amberlith-0.18.0-0123abc.apk",
                "vpk": "amberlith-0.18.0-0123abc.vpk",
                "3dsx": "amberlith-0.18.0-0123abc.3dsx",
                "nds": "amberlith-0.18.0-0123abc.nds",
                "ounce": "amberlith-ounce-0.18.0-0123abc.zip",
                "mod": "amberlith-mod-0.18.0-0123abc.zip",
            },
        )

    def test_the_nro_its_size_and_checksum(self):
        self.assertIn(f"| [{NRO}]({WHERE}/{NRO}) | 1.8 MB | `{DIGEST}` |", self.text)
        self.assertIn(f"[{NRO}.sha256]({WHERE}/{NRO}.sha256)", self.text)
        self.assertIn(f"`sha256sum -c {NRO}.sha256` beside it checks it", self.text)
        self.assertIn(f"{URL}/blob/{self.sha}/docs/Releases.md", self.text)

    def test_the_apk_the_vpk_the_3dsx_the_nds_and_the_exefs_zips(self):
        built = [
            nightly.Built("nro", NRO, 1, DIGEST),
            nightly.Built("apk", NAMES["apk"], 9_000_000, "01" * 32),
            nightly.Built("vpk", NAMES["vpk"], 2_400_000, "ab" * 32),
            nightly.Built("3dsx", NAMES["3dsx"], 2_200_000, "12" * 32),
            nightly.Built("nds", NAMES["nds"], 750_000, "34" * 32),
            nightly.Built("ounce", NAMES["ounce"], 650_000, "cd" * 32),
            nightly.Built("mod", NAMES["mod"], 210_000, "ef" * 32),
        ]
        text = nightly.notes(self.checkout, self.sha, "", "owner/repo", WHERE, built, "")
        ounce, mod = NAMES["ounce"], NAMES["mod"]
        self.assertIn(f"| [{ounce}]({WHERE}/{ounce}) | 0.7 MB | `{'cd' * 32}` |", text)
        self.assertIn(f"| [{mod}]({WHERE}/{mod}) | 0.2 MB | `{'ef' * 32}` |", text)
        # every file's row in the order the release lists them, the NRO first
        rows = [line.split("]")[0][3:] for line in text.splitlines() if line.startswith("| [")]
        self.assertEqual(rows, [NRO, NAMES["apk"], NAMES["vpk"], NAMES["3dsx"], NAMES["nds"], ounce, mod])
        self.assertIn(
            f"`sha256sum -c {NRO}.sha256 {NAMES['apk']}.sha256 {NAMES['vpk']}.sha256 {NAMES['3dsx']}.sha256 "
            f"{NAMES['nds']}.sha256 {ounce}.sha256 {mod}.sha256` beside them checks them all.",
            text,
        )
        self.assertIn(f"| [{NAMES['vpk']}]({WHERE}/{NAMES['vpk']}) | 2.4 MB | `{'ab' * 32}` |", text)
        self.assertIn(f"{NAMES['vpk']} {nightly.RUNS['vpk']}", text)
        self.assertIn(f"[Vita.md]({URL}/blob/{self.sha}/docs/Vita.md)", text)
        self.assertIn("- The VPK was built with the VitaSDK .github/workflows/build.yml names", text)
        self.assertIn(f"| [{NAMES['3dsx']}]({WHERE}/{NAMES['3dsx']}) | 2.2 MB | `{'12' * 32}` |", text)
        self.assertIn(f"{NAMES['3dsx']} {nightly.RUNS['3dsx']}", text)
        self.assertIn(f"[3DS.md]({URL}/blob/{self.sha}/docs/3DS.md)", text)
        self.assertIn("- The 3DSX was built in the devkitARM container .github/workflows/build.yml names", text)
        self.assertIn(f"| [{NAMES['nds']}]({WHERE}/{NAMES['nds']}) | 0.8 MB | `{'34' * 32}` |", text)
        self.assertIn(f"{NAMES['nds']} {nightly.RUNS['nds']}", text)
        self.assertIn(f"[DS.md]({URL}/blob/{self.sha}/docs/DS.md)", text)
        self.assertIn("- The NDS was built in the same devkitARM container, and links libnds (zlib) and calico", text)
        self.assertIn(f"{mod} {nightly.RUNS['mod']}", text)
        self.assertIn(f"[Mod.md]({URL}/blob/{self.sha}/docs/Mod.md)", text)
        self.assertIn(f"[Ounce.md]({URL}/blob/{self.sha}/docs/Ounce.md)", text)
        self.assertIn(f"- {ounce} and {mod} were built in the same container as the NRO, and link libnx", text)
        # a release with the NRO alone names no other document nor licence
        self.assertNotIn("Mod.md", self.text)
        self.assertNotIn("Vita.md", self.text)
        self.assertNotIn("VitaSDK", self.text)
        self.assertNotIn("3DS.md", self.text)
        self.assertNotIn("libctru", self.text)
        self.assertNotIn("DS.md", self.text)
        self.assertNotIn("calico", self.text)
        self.assertNotIn("amberlith-mod", self.text)

    def test_changes_counted_and_linked(self):
        self.assertIn(f"- \\[ci\\] its title ([#5]({URL}/pull/5))", self.text)
        self.assertIn(
            f"[`{self.previous[:7]}...{self.sha[:7]}`]({URL}/compare/{self.previous}...{self.sha}): 2 changes, "
            "the newest first.",
            self.text,
        )

    def test_source_folded_at_the_end(self):
        self.assertIn(f"{URL}/tree/{self.sha}", self.text)
        self.assertGreater(self.text.index("<details>"), self.text.index("## Changes"))
        self.assertTrue(self.text.rstrip().endswith("</details>"))

    def test_what_becomes_of_it_and_the_lead(self):
        self.assertIn("committed 2026-10-15. Kept.", self.text)
        lead = nightly.notes(
            self.checkout, self.sha, "", "owner/repo", WHERE, ALONE, "", lead="Amberlith v1, built from"
        )
        self.assertTrue(lead.startswith(f"Amberlith v1, built from [`{self.sha[:7]}`]"))
        self.assertNotIn("## Changes", lead)

    def test_the_same_commit_again(self):
        again = nightly.notes(self.checkout, self.sha, self.sha, "owner/repo", WHERE, ALONE, "Kept.")
        self.assertIn("None: the same commit", again)

    def test_a_long_list_cut(self):
        world = World(os.path.join(self.temporary.name, "long"), "owner/repo")
        first = world.commit("first")
        for n in range(nightly.MOST_CHANGES + 2):
            last = world.commit(f"commit {n}")
        many = nightly.notes(str(world.checkout), last, first, "owner/repo", WHERE, ALONE, "")
        listed = many.split("## Changes")[1].split("<details>")[0].count("\n- ")
        self.assertEqual(listed, nightly.MOST_CHANGES + 1)
        self.assertIn("- and [2 more](", many)


if __name__ == "__main__":
    unittest.main()
