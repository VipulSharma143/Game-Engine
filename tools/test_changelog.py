#!/usr/bin/env python3
"""Tests for changelog.py. Run: python3 tools/test_changelog.py"""
import contextlib
import io
import tempfile
import unittest
from pathlib import Path

import changelog


def entry(version, date, bullet="Something changed."):
    return f"## [{version}] - {date}\n\n### Features\n\n- {bullet}\n"


def doc(*entries, title="# Changelog"):
    return title + "\n\nIntro text.\n\n" + "\n".join(entries)


class ChangelogTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)
        self.set_cmake("0.3.0")

    def set_cmake(self, version):
        (self.root / "CMakeLists.txt").write_text(f"project(SORTED VERSION {version} LANGUAGES CXX)\n")

    def write(self, name, text):
        (self.root / name).write_text(text)

    def read(self, name):
        return (self.root / name).read_text()

    def run_cmd(self, *args):
        err = io.StringIO()
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            code = changelog.main(["--root", str(self.root), *args])
        return code, err.getvalue()

    def three_releases(self):
        self.write("CHANGELOG.md", doc(entry("0.3.0", "2026-03-01"), entry("0.2.0", "2026-02-01", "Second."),
                                        entry("0.1.0", "2026-01-01", "First.")))

    def test_sync_moves_oldest_verbatim_and_is_idempotent(self):
        self.three_releases()
        self.assertEqual(self.run_cmd("sync")[0], 0)
        self.assertNotIn("0.1.0", self.read("CHANGELOG.md"))
        self.assertIn(entry("0.1.0", "2026-01-01", "First."), self.read("ARCHIVE_CHANGELOG.md"))
        snapshot = (self.read("CHANGELOG.md"), self.read("ARCHIVE_CHANGELOG.md"))
        self.assertEqual(self.run_cmd("sync")[0], 0)
        self.assertEqual(snapshot, (self.read("CHANGELOG.md"), self.read("ARCHIVE_CHANGELOG.md")))

    def test_archive_keeps_older_history_in_order(self):
        self.write("ARCHIVE_CHANGELOG.md", doc(entry("0.0.9", "2025-12-01"), title="# Archived Changelog"))
        self.three_releases()
        self.assertEqual(self.run_cmd("sync")[0], 0)
        archive = self.read("ARCHIVE_CHANGELOG.md")
        self.assertLess(archive.index("[0.1.0]"), archive.index("[0.0.9]"))

    def test_interrupted_run_is_recovered_without_duplicates(self):
        self.three_releases()
        self.write("ARCHIVE_CHANGELOG.md", doc(entry("0.1.0", "2026-01-01", "First."), title="# Archived Changelog"))
        self.assertEqual(self.run_cmd("sync")[0], 0)
        self.assertEqual(self.read("ARCHIVE_CHANGELOG.md").count("[0.1.0]"), 1)

    def test_version_in_both_files_with_different_text_is_rejected(self):
        self.three_releases()
        self.write("ARCHIVE_CHANGELOG.md", doc(entry("0.1.0", "2026-01-01", "Other."), title="# Archived Changelog"))
        code, err = self.run_cmd("sync")
        self.assertEqual(code, 1)
        self.assertIn("different text", err)

    def test_cmake_version_mismatch_is_rejected(self):
        self.write("CHANGELOG.md", doc(entry("0.3.0", "2026-03-01"), entry("0.2.0", "2026-02-01")))
        self.set_cmake("0.4.0")
        code, err = self.run_cmd("check")
        self.assertEqual(code, 1)
        self.assertIn("0.4.0", err)

    def test_bad_heading_order_and_duplicates_are_rejected(self):
        for body in (doc(entry("0.1.0", "2026-01-01"), entry("0.3.0", "2026-03-01")),
                     doc(entry("0.3.0", "2026-03-01"), entry("0.3.0", "2026-03-01")),
                     doc("## Version 0.3.0\n\n### Features\n\n- x\n"),
                     doc("## [0.3.0] - 2026-13-45\n\n### Features\n\n- x\n"),
                     doc("## [0.3.0] - 2026-03-01\n\n### Stuff\n\n- x\n"),
                     doc("## [0.3.0] - 2026-03-01\n\n### Features\n\n")):
            self.write("CHANGELOG.md", body)
            self.assertEqual(self.run_cmd("check")[0], 1, body)

    def test_check_does_not_modify_files(self):
        self.three_releases()
        before = self.read("CHANGELOG.md")
        self.assertEqual(self.run_cmd("check")[0], 1)  # three releases: a sync is needed
        self.assertEqual(self.read("CHANGELOG.md"), before)
        self.assertFalse((self.root / "ARCHIVE_CHANGELOG.md").exists())

    def test_release_stamps_unreleased_and_requires_cmake_match(self):
        self.write("CHANGELOG.md", doc(entry("0.4.0", "Unreleased"), entry("0.3.0", "2026-03-01")))
        self.assertEqual(self.run_cmd("release", "0.4.0", "--date", "2026-04-01")[0], 1)  # CMake still 0.3.0
        self.assertIn("Unreleased", self.read("CHANGELOG.md"))
        self.set_cmake("0.4.0")
        self.assertEqual(self.run_cmd("release", "0.4.0", "--date", "2026-04-01")[0], 0)
        self.assertIn("## [0.4.0] - 2026-04-01", self.read("CHANGELOG.md"))
        self.assertEqual(self.run_cmd("release", "0.4.0", "--date", "2026-09-09")[0], 0)  # rerun keeps the first date
        self.assertIn("## [0.4.0] - 2026-04-01", self.read("CHANGELOG.md"))

    def test_release_of_missing_version_is_rejected(self):
        self.three_releases()
        code, err = self.run_cmd("release", "0.9.0")
        self.assertEqual(code, 1)
        self.assertIn("0.9.0", err)

    def test_tag_validation(self):
        self.write("CHANGELOG.md", doc(entry("0.3.0", "2026-03-01")))
        self.assertEqual(self.run_cmd("check", "--tag", "v0.3.0")[0], 0)
        self.assertEqual(self.run_cmd("check", "--tag", "v0.3.1")[0], 1)
        self.assertEqual(self.run_cmd("check", "--tag", "0.3.0")[0], 1)
        self.write("CHANGELOG.md", doc(entry("0.3.0", "Unreleased")))
        self.assertEqual(self.run_cmd("check", "--tag", "v0.3.0")[0], 1)

    def test_notes(self):
        self.write("CHANGELOG.md", doc(entry("0.3.0", "2026-03-01", "Hello.")))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(changelog.main(["--root", str(self.root), "notes", "0.3.0"]), 0)
        self.assertIn("- Hello.", out.getvalue())
        self.assertNotIn("## [", out.getvalue())


if __name__ == "__main__":
    unittest.main()
