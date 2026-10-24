#!/usr/bin/env python3
"""Validate and synchronise CHANGELOG.md and ARCHIVE_CHANGELOG.md (standard library only).

  changelog.py sync              keep the two newest releases in CHANGELOG.md, move older ones to the archive
  changelog.py check [--tag vX]  validate only (changes nothing); with --tag also validate a release tag
  changelog.py release X.Y.Z     give the "Unreleased" entry for X.Y.Z today's date, then sync
  changelog.py notes X.Y.Z       print the release notes of X.Y.Z (used for the GitHub Release body)

The authoritative version is `project(GameEngine VERSION x.y.z ...)` in CMakeLists.txt. Entries are moved
verbatim, nothing is ever dropped, and running a command twice changes nothing the second time.
"""
import argparse
import datetime
import os
import re
import sys
import tempfile
from pathlib import Path

KEEP = 2  # released versions kept in CHANGELOG.md
CATEGORIES = ("Features", "Improvements", "Bug Fixes", "Breaking Changes")
HEADING = re.compile(r"^## \[(\d+\.\d+\.\d+)\] - (\d{4}-\d{2}-\d{2}|Unreleased)$")
CMAKE_VERSION = re.compile(r"^\s*project\(\s*GameEngine\s+VERSION\s+(\d+\.\d+\.\d+)[\s)]", re.M)
TAG = re.compile(r"^v(\d+\.\d+\.\d+)$")
ARCHIVE_PREAMBLE = [
    "# Archived Changelog",
    "",
    "Release notes older than the two newest releases, newest first. `tools/changelog.py` moves them here from",
    "[CHANGELOG.md](CHANGELOG.md) unchanged; nothing in this file is ever deleted.",
]


class ChangelogError(Exception):
    pass


class Entry:
    def __init__(self, version, date, lines, line_no):
        self.version, self.date, self.lines, self.line_no = version, date, lines, line_no

    @property
    def key(self):
        return tuple(int(p) for p in self.version.split("."))

    @property
    def released(self):
        return self.date != "Unreleased"

    @property
    def text(self):
        return "\n".join(self.lines)


def parse(name, text):
    """Split a changelog into (preamble lines, entries). Every '## ' line must be a valid release heading."""
    lines = text.split("\n")
    starts = [i for i, line in enumerate(lines) if line.startswith("## ")]
    preamble = lines[: starts[0] if starts else len(lines)]
    entries = []
    for n, start in enumerate(starts):
        block = lines[start : starts[n + 1] if n + 1 < len(starts) else len(lines)]
        while block and not block[-1].strip():
            block.pop()
        match = HEADING.match(block[0])
        if not match:
            raise ChangelogError(f"{name}:{start + 1}: invalid release heading {block[0]!r}; "
                                 "expected '## [X.Y.Z] - YYYY-MM-DD' (or '- Unreleased')")
        version, date = match.groups()
        if date != "Unreleased":
            try:
                datetime.date.fromisoformat(date)
            except ValueError:
                raise ChangelogError(f"{name}:{start + 1}: {date} is not a real date") from None
        entries.append(Entry(version, date, block, start + 1))
    return preamble, entries


def check_body(name, entry):
    """CHANGELOG.md entries need categorized bullets; archived history is kept as written."""
    seen, with_bullets = [], set()
    for offset, line in enumerate(entry.lines[1:], start=1):
        where = f"{name}:{entry.line_no + offset}"
        if line.startswith("### "):
            category = line[4:].strip()
            if category not in CATEGORIES:
                raise ChangelogError(f"{where}: unknown category {category!r}; use one of: {', '.join(CATEGORIES)}")
            if category in seen:
                raise ChangelogError(f"{where}: category {category!r} appears twice in {entry.version}")
            seen.append(category)
        elif line.startswith("#"):
            raise ChangelogError(f"{where}: only '### <category>' headings are allowed inside a release")
        elif line.strip():
            if not seen:
                raise ChangelogError(f"{where}: text before the first '### <category>' heading")
            if line.lstrip().startswith(("- ", "* ")):
                with_bullets.add(seen[-1])
    if not seen:
        raise ChangelogError(f"{name}:{entry.line_no}: release {entry.version} has no categories")
    for category in seen:
        if category not in with_bullets:
            raise ChangelogError(f"{name}:{entry.line_no}: category {category!r} of {entry.version} has no entries")


def check_order(name, entries):
    for newer, older in zip(entries, entries[1:]):
        if newer.key <= older.key:
            raise ChangelogError(f"{name}:{older.line_no}: {older.version} must be older than {newer.version} "
                                 "(newest first, no duplicates)")


def check_all(cl, ar):
    """Validate the final state of both files together."""
    check_order("CHANGELOG.md", cl)
    check_order("ARCHIVE_CHANGELOG.md", ar)
    for i, e in enumerate(cl):
        if not e.released and i != 0:
            raise ChangelogError(f"CHANGELOG.md:{e.line_no}: only the newest entry may be Unreleased")
        check_body("CHANGELOG.md", e)
    for e in ar:
        if not e.released:
            raise ChangelogError(f"ARCHIVE_CHANGELOG.md:{e.line_no}: archived releases need a date")
    both = [e.version for e in cl] + [e.version for e in ar]
    for version in sorted(set(both)):
        if both.count(version) > 1:
            raise ChangelogError(f"version {version} appears in both CHANGELOG.md and ARCHIVE_CHANGELOG.md")
    ordered = [e for e in cl + ar if e.released]
    if ar and cl and ar[0].key >= cl[-1].key:
        raise ChangelogError(f"ARCHIVE_CHANGELOG.md has {ar[0].version}, which is not older than {cl[-1].version}")
    for newer, older in zip(ordered, ordered[1:]):
        if newer.date < older.date:
            raise ChangelogError(f"{newer.version} is dated {newer.date}, earlier than the older {older.version} "
                                 f"({older.date})")


def organise(cl, ar):
    """Keep the Unreleased entry and the KEEP newest releases; prepend the rest to the archive."""
    released = [e for e in cl if e.released]
    moved = released[KEEP:]
    archived = {e.version: e for e in ar}
    to_add = []
    for e in moved:
        old = archived.get(e.version)
        if old is None:
            to_add.append(e)
        elif old.text != e.text:  # an identical copy is what an interrupted earlier run leaves behind
            raise ChangelogError(f"{e.version} is in both files with different text; resolve it by hand")
    moved_versions = {e.version for e in moved}
    return [e for e in cl if e.version not in moved_versions], to_add + ar


def render(preamble, entries):
    head = list(preamble)
    while head and not head[-1].strip():
        head.pop()
    parts = ["\n".join(head)] + [e.text for e in entries]
    return "\n\n".join(parts) + "\n"


def write_if_changed(path, new_text):
    if path.exists() and path.read_text(encoding="utf-8") == new_text:
        return False
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name, suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as f:
        f.write(new_text)
    os.replace(tmp, path)
    return True


def cmake_version(root):
    try:
        text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    except OSError as e:
        raise ChangelogError(f"cannot read CMakeLists.txt: {e}") from None
    match = CMAKE_VERSION.search(text)
    if not match:
        raise ChangelogError("CMakeLists.txt has no 'project(GameEngine VERSION X.Y.Z ...)' line")
    return match.group(1)


def check_versions(cl, version, tag=None):
    released = [e for e in cl if e.released]
    expected = released[0] if released else cl[0]
    if expected.version != version:
        raise ChangelogError(f"CMakeLists.txt says {version} but the newest changelog release is {expected.version}")
    if tag is not None:
        match = TAG.match(tag)
        if not match:
            raise ChangelogError(f"tag {tag!r} is not of the form vMAJOR.MINOR.PATCH")
        if match.group(1) != version:
            raise ChangelogError(f"tag {tag} does not match CMakeLists.txt version {version}")
        if not released or released[0].version != version or cl[0] is not released[0]:
            raise ChangelogError(f"CHANGELOG.md has no dated entry for {version} as its newest release")


def run(args):
    root = Path(args.root).resolve()
    cl_path, ar_path = root / "CHANGELOG.md", root / "ARCHIVE_CHANGELOG.md"
    if not cl_path.exists():
        raise ChangelogError("CHANGELOG.md does not exist")
    cl_text = cl_path.read_text(encoding="utf-8")
    ar_text = ar_path.read_text(encoding="utf-8") if ar_path.exists() else None
    cl_pre, cl = parse("CHANGELOG.md", cl_text)
    ar_pre, ar = parse("ARCHIVE_CHANGELOG.md", ar_text) if ar_text is not None else (ARCHIVE_PREAMBLE, [])
    if not cl:
        raise ChangelogError("CHANGELOG.md contains no releases")
    if not cl_pre or cl_pre[0].strip() != "# Changelog":
        raise ChangelogError("CHANGELOG.md must start with '# Changelog'")
    if ar_text is not None and (not ar_pre or ar_pre[0].strip() != "# Archived Changelog"):
        raise ChangelogError("ARCHIVE_CHANGELOG.md must start with '# Archived Changelog'")
    version = cmake_version(root)

    if args.command == "notes":
        for e in cl + ar:
            if e.version == args.version and e.released:
                print("\n".join(e.lines[1:]).strip())
                return
        raise ChangelogError(f"no dated release notes for {args.version}")

    # validate the files as they are before touching anything (notes only needs a parse)
    before = {e.version: e.text for e in cl + ar}
    check_order("CHANGELOG.md", cl)
    check_order("ARCHIVE_CHANGELOG.md", ar)

    if args.command == "release":
        if cl[0].version != args.version:
            raise ChangelogError(f"{args.version} must be the newest entry in CHANGELOG.md "
                                 f"(found {cl[0].version}); add '## [{args.version}] - Unreleased'")
        if not cl[0].released:
            cl[0].date = args.date
            cl[0].lines[0] = f"## [{args.version}] - {args.date}"
            before[args.version] = cl[0].text

    new_cl, new_ar = organise(cl, ar)
    check_all(new_cl, new_ar)
    if args.command == "check":
        if len([e for e in cl if e.released]) > KEEP:
            raise ChangelogError(f"CHANGELOG.md holds more than {KEEP} releases; run tools/changelog.py sync")
        check_versions(cl, version, args.tag)
        if args.tag and not cl[0].released:
            raise ChangelogError(f"{cl[0].version} is still Unreleased")
        print(f"changelog ok (version {version})")
        return
    check_versions(new_cl, version)

    # nothing may be lost: every release must come out exactly as it went in
    after = {e.version: e.text for e in new_cl + new_ar}
    if after != before:
        raise ChangelogError("internal error: the sync would change or lose release notes; nothing was written")

    changed = [write_if_changed(ar_path, render(ar_pre, new_ar)), write_if_changed(cl_path, render(cl_pre, new_cl))]
    moved = len(cl) - len(new_cl)
    print(f"changelog {'updated' if any(changed) else 'already in sync'}: "
          f"{len(new_cl)} in CHANGELOG.md, {len(new_ar)} in ARCHIVE_CHANGELOG.md, {moved} moved")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--root", default=Path(__file__).resolve().parent.parent, help="repository root")
    sub = p.add_subparsers(dest="command", required=True)
    sub.add_parser("sync")
    c = sub.add_parser("check")
    c.add_argument("--tag")
    r = sub.add_parser("release")
    r.add_argument("version")
    r.add_argument("--date", default=datetime.date.today().isoformat())
    n = sub.add_parser("notes")
    n.add_argument("version")
    args = p.parse_args(argv)
    try:
        run(args)
    except ChangelogError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
