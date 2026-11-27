# SPDX-FileCopyrightText: Copyright 2026 Amberlith contributors
# SPDX-License-Identifier: GPL-3.0-or-later
"""publish-builds.py against fake_github.py's fake gh and fake Discord: which Build runs are published; a nightly
prerelease with the NRO, the APK, the VPK, the 3DSX and the two ExeFS zips and their checksums, named after the commit's
VERSION and the commit, kept to the newest N, never one older than the newest, its files and notes copied to the "nightly" prerelease
whose tag follows it, and nothing said on Discord; a version tag's release named after its version alone, the latest,
and its Discord message, or a notice without the webhook; an artifact that is missing, is no NRO, or a zip that holds
other files or a file of another kind, a VERSION that is no version and a tag that is not its VERSION's, stopping the
run. Offline."""

import hashlib
import io
import json
import os
import re
import tempfile
import unittest
import zipfile
from pathlib import Path

from fake_github import Discord, World, python, serve

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(HERE, "publish-builds.py")
WORKFLOWS = Path(HERE).parent / "workflows"
REPO = "owner/repo"
NRO = b"\0" * 16 + b"NRO0" + b"\0" * 12 + b"the program"
APK = b"PK\x03\x04" + b"\0" * 26 + b"the app"
MODULE = b"NSO0" + b"\0" * 12 + b"a module"
DESCRIPTOR = b"META" + b"\0" * 12 + b"a descriptor"


def exefs(files, folder=True):
    """a zip of an ExeFS folder as build.yml makes it: the folder's own entry, then each file in it"""
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w") as packed:
        if folder:
            packed.writestr("exefs/", b"")
        for name, data in files.items():
            packed.writestr(name, data)
    return buffer.getvalue()


OUNCE = exefs({"exefs/main": MODULE, "exefs/main.npdm": DESCRIPTOR})
MOD = exefs({"exefs/subsdk9": MODULE, "exefs/main.npdm": DESCRIPTOR})
# the Vita's program, a SELF, and its title's details, an SFO, as vita.mk packs them
VPK = exefs(
    {"sce_sys/param.sfo": b"\x00PSF" + b"\0" * 12 + b"the title", "eboot.bin": b"SCE\x00" + b"\0" * 12 + b"a SELF"},
    folder=False,
)
# a 3DSX as 3dsxtool writes it: its header's magic, then the program
THREEDSX = b"3DSX" + b"\0" * 28 + b"a program"


def named(stem):
    """the six files of a release or a run named after stem (a version, or a version and a commit), as build.yml and
    publish-builds.py name them"""
    return {
        "nro": f"amberlith-{stem}.nro",
        "apk": f"amberlith-{stem}.apk",
        "vpk": f"amberlith-{stem}.vpk",
        "3dsx": f"amberlith-{stem}.3dsx",
        "ounce": f"amberlith-ounce-{stem}.zip",
        "mod": f"amberlith-mod-{stem}.zip",
    }


def files(stem):
    """every asset of a release named after stem: the six files and their checksums, sorted"""
    return sorted(name + more for name in named(stem).values() for more in ("", ".sha256"))


server = HOOK = None


def setUpModule():
    global server, HOOK
    server, HOOK = serve()


def tearDownModule():
    server.shutdown()
    server.server_close()


def sent(channel, mark):
    """what reached the channel's webhook since mark (a count of Discord's requests)"""
    return [r for r in Discord.requests[mark:] if f"token-{channel}" in r["path"]]


class Publish(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        world = self.world = World(self.temporary.name, REPO)
        (world.checkout / "VERSION").write_text("0.2.0\n")
        world.git("add", "VERSION")
        self.oldest = world.commit("the older nightly")
        self.previous = world.commit("the previous nightly")
        world.git("tag", "v0.1.0")
        world.git("switch", "-q", "-c", "feature")
        world.commit("amberlith: a commit of the pull request")
        world.git("switch", "-q", "main")
        world.git(
            "merge", "-q", "--no-ff", "feature", "-m", "Merge pull request #5 from someone/feature\n\n[ci] its title"
        )
        self.sha = world.git("rev-parse", "HEAD")
        world.git("tag", "v0.2.0")
        self.tag = f"nightly-2026-10-15-{self.sha[:7]}"
        self.stem = f"0.2.0-{self.sha[:7]}"
        self.rolling = {
            "id": 9,
            "tag_name": "nightly",
            "name": "old",
            "created_at": "2026-09-01",
            "body": "old",
            "prerelease": True,
            "assets": [{"id": 7, "name": "amberlith.nro", "data": "old"}, {"id": 8, "name": "gone.txt", "data": ""}],
        }
        world.save(
            {
                "next": 100,
                "clock": 0,
                "pulls": [],
                "runs": [],
                "tags": {
                    f"nightly-2026-10-15-{self.oldest[:7]}": "x",
                    f"nightly-2026-10-15-{self.previous[:7]}": "x",
                    "nightly": self.oldest,
                    "v0.1.0": self.previous,
                    "v0.2.0": self.sha,
                },
                "releases": [
                    self.nightly_release(self.oldest, "2026-10-01", 1),
                    self.nightly_release(self.previous, "2026-10-02", 2),
                    self.rolling,
                ],
            }
        )

    @staticmethod
    def nightly_release(commit, published, number):
        return {
            "id": number,
            "tag_name": f"nightly-2026-10-15-{commit[:7]}",
            "name": "n",
            "created_at": "2026-10-01T00:00:00Z",  # GitHub's: the tag's commit's date
            "published_at": published,
            "body": f"Built from [`{commit[:7]}`](https://github.com/{REPO}/commit/{commit}).",
            "assets": [],
            "prerelease": True,
        }

    # ---- helpers
    def find(self, tag):
        return next((r for r in self.world.state()["releases"] if r["tag_name"] == tag), None)

    def artifacts(
        self,
        name,
        commit,
        data=NRO,
        as_file=False,
        apk=APK,
        ounce=OUNCE,
        mod=MOD,
        version="0.2.0",
        vpk=VPK,
        threedsx=THREEDSX,
    ):
        """the folder the workflow downloads into: the NRO, the APK, the VPK, the 3DSX and the two zips, named after the version and the
        commit, each in a folder named after its artifact, or at its path; a dict for a file is the files a download
        that unpacked it leaves in that folder"""
        artifact = named(f"{version}-{commit[:7]}")
        for file, content in (
            (artifact["nro"], data),
            (artifact["apk"], apk),
            (artifact["vpk"], vpk),
            (artifact["3dsx"], threedsx),
            (artifact["ounce"], ounce),
            (artifact["mod"], mod),
        ):
            path = self.world.root / name / file
            if isinstance(content, dict):
                for member, data in content.items():
                    (path / member).parent.mkdir(parents=True, exist_ok=True)
                    (path / member).write_bytes(data)
            elif as_file:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
            elif content is not None:
                path.mkdir(parents=True, exist_ok=True)
                (path / file).write_bytes(content)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
        return str(self.world.root / name)

    def run_script(self, step, *extra, event=None, name="workflow_run", hooks=True):
        event_file, output = self.world.root / "event.json", self.world.root / "output"
        event_file.write_text(json.dumps(event or {}))
        output.write_text("")
        env = {"GITHUB_EVENT_NAME": name, "GITHUB_EVENT_PATH": str(event_file), "GITHUB_OUTPUT": str(output)}
        if hooks:
            env.update(DISCORD_WEBHOOK_RELEASES=f"{HOOK}-releases")
        checkout = ["--checkout", str(self.world.checkout)] if step == "plan" else []
        done = self.world.run(python(SCRIPT, step, "--repo", REPO, *checkout, *extra), **env)
        planned = dict(line.split("=", 1) for line in output.read_text().splitlines())
        return done, planned

    def publish(
        self,
        kind,
        commit,
        *extra,
        run="41",
        data=NRO,
        as_file=False,
        hooks=True,
        apk=APK,
        ounce=OUNCE,
        mod=MOD,
        version="0.2.0",
        vpk=VPK,
        threedsx=THREEDSX,
    ):
        folder = self.artifacts(f"artifacts-{run}", commit, data, as_file, apk, ounce, mod, version, vpk, threedsx)
        args = [
            "--kind",
            kind,
            "--run",
            run,
            "--sha",
            commit,
            "--artifacts",
            folder,
            "--checkout",
            str(self.world.checkout),
        ]
        return self.run_script("publish", *args, *extra, hooks=hooks)[0]

    def push_run(self, commit, conclusion="success", branch="main", number=41, repo=REPO, event="push"):
        return {
            "workflow_run": {
                "id": number,
                "event": event,
                "head_branch": branch,
                "head_sha": commit,
                "conclusion": conclusion,
                "created_at": "2026-10-16T10:00:00Z",
                "head_repository": {"full_name": repo},
            }
        }

    # ---- what plan publishes
    def test_a_green_push_to_main_is_planned_as_a_nightly(self):
        done, planned = self.run_script("plan", event=self.push_run(self.sha))
        self.assertEqual(
            planned,
            {"kind": "nightly", "run": "41", "sha": self.sha, "short": self.sha[:7], "tag": "", "stem": self.stem},
            done.stdout + done.stderr,
        )

    def test_a_version_tag_pushed_is_planned_as_a_release(self):
        done, planned = self.run_script("plan", event=self.push_run(self.sha, branch="v0.2.0"))
        self.assertEqual(
            (planned.get("kind"), planned.get("tag"), planned.get("stem")),
            ("release", "v0.2.0", self.stem),
            done.stdout + done.stderr,
        )

    def test_what_is_not_planned(self):
        for event, why in (
            (self.push_run(self.sha, "failure"), "a failed run"),
            (self.push_run(self.sha, branch="dev"), "another branch"),
            (self.push_run(self.sha, branch="v9.9.9"), "a branch named like a version, no tag"),
            (self.push_run(self.sha, repo="someone/fork"), "a push to a fork"),
            # its branch named as a version tag that exists: only the event tells it from the tag's push
            (self.push_run(self.sha, event="pull_request", branch="v0.2.0"), "a pull request's run"),
        ):
            with self.subTest(why):
                done, planned = self.run_script("plan", event=event)
                self.assertEqual((planned, done.returncode), ({}, 0), done.stdout + done.stderr)
        done, planned = self.run_script("plan", name="push", event={})
        self.assertEqual((planned, done.returncode), ({}, 0), done.stdout + done.stderr)

    # ---- a nightly
    def test_a_nightly_with_the_nro_and_its_checksum(self):
        mark = len(Discord.requests)
        done = self.publish("nightly", self.sha, "--keep", "2")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        made = self.find(self.tag)
        self.assertTrue(made["prerelease"])
        self.assertEqual(made["make_latest"], "false")  # the latest is a version's
        data = {a["name"]: a["data"] for a in made["assets"]}
        # named after the commit's VERSION and the commit
        self.assertEqual(sorted(data), files(self.stem))
        where = f"https://github.com/{REPO}/releases/download/{self.tag}"
        body = made["body"]
        name = named(self.stem)
        # each file as it was built, its checksum and its row
        for kind, built in (
            ("nro", NRO),
            ("apk", APK),
            ("vpk", VPK),
            ("3dsx", THREEDSX),
            ("ounce", OUNCE),
            ("mod", MOD),
        ):
            self.assertEqual(data[name[kind]].encode("latin-1"), built)
            built_digest = hashlib.sha256(built).hexdigest()
            self.assertEqual(data[f"{name[kind]}.sha256"], f"{built_digest}  {name[kind]}\n")
            self.assertIn(f"| [{name[kind]}]({where}/{name[kind]}) | 0.0 MB | `{built_digest}` |", body)
        self.assertIn(
            f"`sha256sum -c {name['nro']}.sha256 {name['apk']}.sha256 {name['vpk']}.sha256 {name['3dsx']}.sha256 "
            f"{name['ounce']}.sha256 {name['mod']}.sha256`",
            body,
        )
        self.assertIn(f"- \\[ci\\] its title ([#5](https://github.com/{REPO}/pull/5))", body)
        self.assertIn(f"[`{self.previous[:7]}...{self.sha[:7]}`]", body)
        self.assertIn("The newest 2 nightlies are kept", body)
        self.assertIn(f"https://github.com/{REPO}/tree/{self.sha}", body)
        state = self.world.state()
        nightlies = sorted(r["tag_name"] for r in state["releases"] if r["tag_name"].startswith("nightly-"))
        self.assertEqual(nightlies, sorted([self.tag, f"nightly-2026-10-15-{self.previous[:7]}"]))
        self.assertNotIn(f"nightly-2026-10-15-{self.oldest[:7]}", state["tags"])
        # the rolling prerelease: the same files, its tag on the commit, the dated one linked
        rolling = self.find("nightly")
        self.assertTrue(rolling["prerelease"])
        self.assertEqual({a["name"]: a["data"] for a in rolling["assets"]}, data)
        self.assertEqual(state["tags"]["nightly"], self.sha)
        self.assertIn(f"https://github.com/{REPO}/releases/tag/{self.tag}", rolling["body"])
        self.assertIn(f"https://github.com/{REPO}/releases/download/nightly/{name['nro']}", rolling["body"])
        # the files of the nightly before, under other names, are gone from it (amberlith.nro, gone.txt)
        self.assertNotIn("amberlith.nro", {a["name"] for a in rolling["assets"]})
        self.assertEqual(rolling["name"], f"Amberlith nightly 2026-10-15 ({self.sha[:7]}), the newest")
        # every write is the repository's, with its token
        writes = [c for c in self.world.calls() if "-X" in c["args"] or c["args"][0] == "release"]
        self.assertTrue(writes)
        self.assertTrue(all(c["token"] == "repo-token" and REPO in " ".join(c["args"]) for c in writes), writes)
        # nothing said on Discord: a nightly has no message
        self.assertEqual(Discord.requests[mark:], [])

    def test_a_nightly_again_replaces_it(self):
        self.publish("nightly", self.sha, "--keep", "2")
        done = self.publish("nightly", self.sha, "--keep", "2")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(len([r for r in self.world.state()["releases"] if r["tag_name"] == self.tag]), 1)

    def test_an_older_commit_is_not_published(self):
        self.publish("nightly", self.sha)
        done = self.publish("nightly", self.oldest, run="40")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("older than the newest nightly", done.stdout)
        self.assertEqual(self.world.state()["tags"]["nightly"], self.sha)

    def test_the_artifact_at_its_own_path(self):
        done = self.publish("nightly", self.sha, as_file=True)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertEqual(sorted(a["name"] for a in self.find(self.tag)["assets"]), files(self.stem))

    def test_a_missing_artifact_stops(self):
        done = self.publish("nightly", self.sha, data=None)
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(f"run 41 built, yet has no amberlith-{self.stem}.nro", done.stderr)
        self.assertIsNone(self.find(self.tag))

    def test_a_file_that_is_no_nro_stops(self):
        done = self.publish("nightly", self.sha, data=b"<html>Not Found</html>")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn("is no NRO: no NRO0 at byte 16", done.stderr)
        self.assertIsNone(self.find(self.tag))

    def test_a_missing_apk_stops(self):
        done = self.publish("nightly", self.sha, apk=None)
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(f"run 41 built, yet has no amberlith-{self.stem}.apk", done.stderr)
        self.assertIsNone(self.find(self.tag))

    def test_a_file_that_is_no_apk_stops(self):
        done = self.publish("nightly", self.sha, apk=b"<html>Not Found</html>")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn("is no APK: no PK\\x03\\x04 at byte 0", done.stderr)
        self.assertIsNone(self.find(self.tag))

    def test_a_file_that_is_no_3dsx_stops(self):
        done = self.publish("nightly", self.sha, threedsx=b"\x7fELF an ELF")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(f"amberlith-{self.stem}.3dsx of run 41 is no 3DSX: no 3DSX at byte 0", done.stderr)
        self.assertIsNone(self.find(self.tag))

    def test_a_vpk_without_the_vitas_program_stops(self):
        for vpk, said in (
            (b"<html>Not Found</html>", f"amberlith-{self.stem}.vpk of run 41 is no VPK: no PK\\x03\\x04 at byte 0"),
            (
                exefs({"eboot.bin": b"SCE\x00 a SELF"}, folder=False),
                f"amberlith-{self.stem}.vpk of run 41 holds eboot.bin, not the VPK's eboot.bin, sce_sys/param.sfo",
            ),
            (
                exefs({"eboot.bin": b"\x7fELF an ELF", "sce_sys/param.sfo": b"\x00PSF"}, folder=False),
                f'eboot.bin in amberlith-{self.stem}.vpk of run 41 starts with "\\x7fELF", not the SCE\\x00 a VPK\'s '
                "eboot.bin starts with",
            ),
        ):
            with self.subTest(said):
                done = self.publish("nightly", self.sha, vpk=vpk)
                self.assertNotEqual(done.returncode, 0)
                self.assertIn(said, done.stderr)
                self.assertIsNone(self.find(self.tag))

    def test_a_missing_zip_stops(self):
        done = self.publish("nightly", self.sha, mod=None)
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(f"run 41 built, yet has no amberlith-mod-{self.stem}.zip", done.stderr)
        self.assertIsNone(self.find(self.tag))

    def test_a_zip_without_its_files_stops(self):
        short = self.stem
        for ounce, mod, said in (
            (
                exefs({"exefs/main": MODULE}),
                MOD,
                f"amberlith-ounce-{short}.zip of run 41 holds exefs/main, not the Switch 2 program's exefs/main, "
                "exefs/main.npdm",
            ),
            (
                OUNCE,
                exefs({"exefs/subsdk9": MODULE, "exefs/main.npdm": DESCRIPTOR, "exefs/main": MODULE}),
                f"amberlith-mod-{short}.zip of run 41 holds exefs/main, exefs/main.npdm, exefs/subsdk9, not the mod of "
                "a game's exefs/main.npdm, exefs/subsdk9",
            ),
            (
                OUNCE,
                exefs({"subsdk9": MODULE, "main.npdm": DESCRIPTOR}, folder=False),
                f"amberlith-mod-{short}.zip of run 41 holds main.npdm, subsdk9, not the mod of a game's "
                "exefs/main.npdm, exefs/subsdk9",
            ),
        ):
            with self.subTest(said):
                done = self.publish("nightly", self.sha, ounce=ounce, mod=mod)
                self.assertNotEqual(done.returncode, 0)
                self.assertIn(said, done.stderr)
                self.assertIsNone(self.find(self.tag))

    def test_a_zip_the_download_unpacked_stops(self):
        # what actions/download-artifact leaves of a zip uploaded as itself when it is not told skip-decompress
        done = self.publish("nightly", self.sha, ounce={"exefs/main": MODULE, "exefs/main.npdm": DESCRIPTOR})
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(
            f"amberlith-ounce-{self.stem}.zip is a folder of 2 files (exefs/main, exefs/main.npdm), not the one Switch "
            "2 program: a download that unpacked the zip leaves its files there",
            done.stderr,
        )
        self.assertIsNone(self.find(self.tag))

    def test_a_zip_with_a_file_of_another_kind_stops(self):
        done = self.publish(
            "nightly", self.sha, mod=exefs({"exefs/subsdk9": b"NRO0 a homebrew", "exefs/main.npdm": DESCRIPTOR})
        )
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(
            f'exefs/subsdk9 in amberlith-mod-{self.stem}.zip of run 41 starts with "NRO0", not the NSO0 a mod of a '
            "game's subsdk9 starts with",
            done.stderr,
        )
        self.assertIsNone(self.find(self.tag))

    def test_a_file_that_is_no_zip_stops(self):
        for ounce, said in (
            (b"<html>Not Found</html>", "is no Switch 2 program: no PK\\x03\\x04 at byte 0"),
            (b"PK\x03\x04 cut short", "is no zip of a Switch 2 program: File is not a zip file"),
        ):
            with self.subTest(said):
                done = self.publish("nightly", self.sha, ounce=ounce)
                self.assertNotEqual(done.returncode, 0)
                self.assertIn(f"amberlith-ounce-{self.stem}.zip of run 41 {said}", done.stderr)
                self.assertIsNone(self.find(self.tag))

    def test_dry_run_writes_nothing(self):
        done = self.publish("nightly", self.sha, "--dry-run")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertFalse([c for c in self.world.calls() if "-X" in c["args"] or c["args"][0] == "release"])
        self.assertIn("would run: gh api -X POST", done.stdout)

    def test_the_rolling_prerelease_made_when_there_is_none(self):
        state = self.world.state()
        state["releases"] = [r for r in state["releases"] if r["tag_name"] != "nightly"]
        del state["tags"]["nightly"]
        self.world.save(state)
        done = self.publish("nightly", self.sha)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        rolling = self.find("nightly")
        self.assertEqual((rolling["target_commitish"], rolling["prerelease"]), (self.sha, True))
        self.assertEqual(sorted(a["name"] for a in rolling["assets"]), files(self.stem))
        self.assertEqual(self.world.state()["tags"]["nightly"], self.sha)

    def test_without_a_webhook_published_and_said(self):
        mark = len(Discord.requests)
        done = self.publish("release", self.sha, "--tag", "v0.2.0", hooks=False)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertIn("::notice::No DISCORD_WEBHOOK_RELEASES", done.stdout)
        self.assertIsNotNone(self.find("v0.2.0"))
        self.assertEqual(Discord.requests[mark:], [])

    # ---- a version
    def add_first_version(self):
        state = self.world.state()
        state["releases"].append(
            {
                "id": 50,
                "tag_name": "v0.1.0",
                "name": "Amberlith v0.1.0",
                "created_at": "2026-10-01",
                "published_at": "2026-10-01",
                "body": "",
                "prerelease": False,
                "assets": [],
            }
        )
        self.world.save(state)

    def test_a_version_release_and_its_message(self):
        self.add_first_version()
        mark = len(Discord.requests)
        done = self.publish("release", self.sha, "--tag", "v0.2.0")
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        version = self.find("v0.2.0")
        self.assertEqual(
            (version["make_latest"], version["prerelease"], version["target_commitish"]), ("true", False, self.sha)
        )
        # named after the version alone
        self.assertEqual(sorted(a["name"] for a in version["assets"]), files("0.2.0"))
        self.assertTrue(version["body"].startswith("Amberlith v0.2.0, built from [`"), version["body"])
        self.assertIn("## Changes since v0.1.0", version["body"])
        self.assertIn(f"https://github.com/{REPO}/releases/download/v0.2.0/amberlith-0.2.0.nro", version["body"])
        posts = sent("releases", mark)
        self.assertEqual(len(posts), 1)
        message = posts[0]["body"]["embeds"][0]
        self.assertEqual(message["title"], "Amberlith v0.2.0")
        self.assertEqual(
            [f["name"] for f in message["fields"]], ["NRO", "APK", "VPK", "3DS", "Switch 2", "Mod", "Source", "Since"]
        )
        self.assertIn("/releases/download/v0.2.0/amberlith-0.2.0.nro", message["fields"][0]["value"])
        self.assertIn("/releases/download/v0.2.0/amberlith-0.2.0.apk", message["fields"][1]["value"])
        self.assertIn("/releases/download/v0.2.0/amberlith-0.2.0.vpk", message["fields"][2]["value"])
        self.assertIn("/releases/download/v0.2.0/amberlith-0.2.0.3dsx", message["fields"][3]["value"])
        self.assertIn("/releases/download/v0.2.0/amberlith-ounce-0.2.0.zip", message["fields"][4]["value"])
        self.assertIn("/releases/download/v0.2.0/amberlith-mod-0.2.0.zip", message["fields"][5]["value"])
        self.assertIn("/compare/v0.1.0...v0.2.0", message["fields"][7]["value"])
        self.assertIn(hashlib.sha256(NRO).hexdigest(), message["description"])
        self.assertEqual(posts[0]["body"]["allowed_mentions"], {"parse": []})
        # its build again: the files replaced, nothing said twice
        done = self.publish("release", self.sha, "--tag", "v0.2.0", run="42")
        self.assertEqual((done.returncode, len(sent("releases", mark))), (0, 1), done.stdout + done.stderr)

    def test_a_tag_with_a_dash_is_a_prerelease(self):
        (self.world.checkout / "VERSION").write_text("0.3.0-rc1\n")
        self.world.git("add", "VERSION")
        candidate_sha = self.world.commit("amberlith: version 0.3.0-rc1")
        self.world.git("tag", "v0.3.0-rc1")
        done = self.publish("release", candidate_sha, "--tag", "v0.3.0-rc1", version="0.3.0-rc1")
        candidate = self.find("v0.3.0-rc1")
        self.assertEqual((candidate["prerelease"], candidate["make_latest"]), (True, "false"), done.stderr)
        self.assertEqual(sorted(a["name"] for a in candidate["assets"]), files("0.3.0-rc1"))

    def test_a_tag_that_is_not_its_version_stops(self):
        self.world.git("tag", "v0.3.0")
        done = self.publish("release", self.sha, "--tag", "v0.3.0")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(f"v0.3.0 is tagged on {self.sha[:7]}, whose VERSION says 0.2.0", done.stderr)
        self.assertIsNone(self.find("v0.3.0"))

    def test_a_version_that_is_no_version_stops(self):
        (self.world.checkout / "VERSION").write_text("next\n")
        self.world.git("add", "VERSION")
        odd = self.world.commit("VERSION says no version")
        said = f'{odd[:7]}\'s VERSION holds "next", not a version'
        done, planned = self.run_script("plan", event=self.push_run(odd))
        self.assertEqual((done.returncode != 0, planned), (True, {}), done.stdout + done.stderr)
        self.assertIn(said, done.stderr)
        done = self.publish("nightly", odd, version="next")
        self.assertNotEqual(done.returncode, 0)
        self.assertIn(said, done.stderr)
        self.assertIsNone(self.find(f"nightly-2026-10-15-{odd[:7]}"))

    def test_a_release_takes_a_version_tag(self):
        done = self.publish("release", self.sha, "--tag", "main")
        self.assertEqual(done.returncode, 2, done.stderr)


def steps(workflow):
    """a workflow's steps, each the text from its "- name:" or "- uses:" to the next one's"""
    return re.split(r"\n(?=\s+- (?:name|uses):)", workflow)


class Workflows(unittest.TestCase):
    """the files go from build.yml to publish-builds.py as they were built"""

    def test_every_file_is_downloaded_as_it_was_uploaded(self):
        uploads = [step for step in steps((WORKFLOWS / "build.yml").read_text()) if "actions/upload-artifact@" in step]
        downloads = [
            step for step in steps((WORKFLOWS / "publish.yml").read_text()) if "actions/download-artifact@" in step
        ]
        # the six files of a release (publish-builds.py's FILES)
        self.assertEqual((len(uploads), len(downloads)), (6, 6))
        for step in uploads:
            self.assertRegex(step, r"(?m)^\s+archive: false$", "build.yml uploads each file itself")
        for step in downloads:
            # download-artifact unpacks a file that is a zip unless told not to
            self.assertRegex(step, r"(?m)^\s+skip-decompress: true$", "publish.yml takes each file as it was uploaded")


if __name__ == "__main__":
    unittest.main()
