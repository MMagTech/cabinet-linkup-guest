#!/usr/bin/env python3
"""
Does a doc or a comment name a file that is not there?

  ci/check-named-files.py

Reads every tracked doc and the comments in every tracked source file, prints each file named that
is not there as file:line, and exits 1 if there is any; 0 when there is none. .github/workflows/
build.yml runs it on every pull request; on GitHub each is also marked on the pull request. REPORT
ONLY UNTIL A DATE: while today (UTC) is not past REPORT_ONLY_UNTIL (YYYY-MM-DD, set by build.yml),
they are warnings and it exits 0.

WHY. A comment or a doc that names a file which moved reads exactly like one that is right, and
CabinetOS's audit found 33 of them after its restructure: the rules that held were the ones CI checks
(docs/decisions/2026-11-12-checks-hold-the-structure.md). This is CabinetOS's check-doc-paths.py and
the file half of its check-code-and-docs-agree.py, cut to this repository.

WHAT IS READ:
  * in a doc (`.md`), a link that is not a web address or an anchor: its target, from the doc's own
    folder, must be there;
  * in a doc, a path in backticks, and in a comment any path, that starts with one of this
    repository's top-level folders (`src/app/app.cpp`, `cabinetos/linkup/rtc.h`), with a line number
    after a colon allowed, must be there;
  * in a comment, a bare file name with a source file's ending (`conductor.cpp`, `build.yml`) must be
    a file here or in the console's code at its pin.
A path is also there when it is in the console's code from CabinetOS's own top (comments name the
console's `docs/SETTINGS.md`), when .gitignore covers it, or when SKIP names it, with why. Code
blocks in docs are not read, and neither is cabinetos/, which is CabinetOS's.

THE CONSOLE'S FILES are read from cabinetos/ as checked out. With the submodule empty, a path in it
counts as there, and a bare name is not read, and the check says so.
"""

import datetime
import os
import posixpath
import re
import subprocess
import sys
import urllib.parse

# What looks like ours and is not in the tree. Each says why.
SKIP = {
    "sound_queue.h",    # made in the build folder by cmake/SoundQueue.cmake
    "sound_queue.cpp",  # the same
    "assets/menubar",   # made beside the program by the build, from icon/menubar*.png
}

# What has comments, and how they are written.
C_LIKE = (".c", ".cc", ".cpp", ".h", ".hpp", ".mm", ".swift", ".in")
HASH = (".sh", ".py", ".cmake", ".yml", ".yaml", ".ps1", ".txt")
HASH_NAMES = ("CMakeLists.txt",)

FENCE = re.compile(r"^\s*(```|~~~)")
CODE = re.compile(r"(?<!`)(`+)(?!`)(.+?)(?<!`)\1(?!`)", re.S)
PARAGRAPH = re.compile(r"(?:[^\n]+\n?)+")
LINK = re.compile(r"\]\(\s*<?([^)\s>]+)>?(?:\s+[\"'(][^)]*)?\)")
REFERENCE = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*<?([^\s>]+)>?", re.M)
SCHEME = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")
LINE_NUMBER = re.compile(r":\d+(-\d+)?$")
NOT_A_PATH = re.compile(r"[\s*?<>{}\[\]$|=()'\",;\\]")
C_TOKEN = re.compile(
    r"//[^\n]*"
    r"|/\*.*?(?:\*/|\Z)"
    r"|\"(?:\\.|[^\"\\\n])*\"?"
    r"|(?<![\w'])'(?:\\.|[^'\\\n])*'?", re.S)
HASH_COMMENT = re.compile(r"(?:^|(?<=\s))#(?![!\[{(])[^\n]*", re.M)
PATH = re.compile(r"(?<![\w/.~$@:%-])(?:\./)?(\.?[A-Za-z_][\w.+-]*(?:/[\w.+-]+)+/?)"
                  r"(?![\w*<{$%(\[]|/)")
BARE = re.compile(r"(?<![\w/.@$%-])([\w+-][\w.+-]*\.(?:cpp|cc|c|h|hpp|mm|swift|sh|py|ps1|cmake|yml))"
                  r"(?![\w/-])")
# Said of a file that has gone, and true of it: "moved out of app.cpp".
HISTORY = re.compile(r"\b(?:out of|was in|were in|old|formerly|used to|moved|split|renamed|replaced)"
                     r"\b.{0,30}$", re.I)


def git(*args, cwd=None):
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True,
                          cwd=cwd).stdout


def listed(cwd=None):
    """Tracked files, and new ones not yet added that .gitignore does not cover."""
    out = git("ls-files", "-z", "--cached", "--others", "--exclude-standard", cwd=cwd)
    return sorted({f for f in out.split("\0") if f})


def line_of(text, at):
    return text.count("\n", 0, at) + 1


def comments(path, text):
    """(offset, comment) for each comment in a source file."""
    name = posixpath.basename(path)
    if name.endswith(C_LIKE):
        return [(m.start(), m.group(0)) for m in C_TOKEN.finditer(text)
                if m.group(0).startswith(("//", "/*"))]
    if name.endswith(HASH) or name in HASH_NAMES:
        return [(m.start(), m.group(0)) for m in HASH_COMMENT.finditer(text)]
    return []


def main():
    os.chdir(git("rev-parse", "--show-toplevel").strip())
    ours = listed()
    console = []
    if os.path.isfile("cabinetos/linkup/noise.cpp"):
        console = ["cabinetos/" + f for f in listed("cabinetos")]
    else:
        print("cabinetos/ is empty: its paths count as there and bare names are not read. "
              "Run: git submodule update --init")
    files = set(ours) | set(console)
    folders = set()
    for f in files:
        while "/" in f:
            f = f.rsplit("/", 1)[0]
            folders.add(f)
    top = {f.split("/", 1)[0] for f in ours if "/" in f} | {"cabinetos"}
    names = {posixpath.basename(f) for f in files}

    def there(path):
        path = path.rstrip("/")
        if path in SKIP:
            return True
        if not console and (path == "cabinetos" or path.startswith("cabinetos/")):
            return True
        return any(p in files or p in folders for p in (path, "cabinetos/" + path))

    def a_path(span):
        span = LINE_NUMBER.sub("", span.strip()).rstrip(".")
        if span.startswith("./"):
            span = span[2:]
        if NOT_A_PATH.search(span) or "/" not in span:
            return None
        if span.split("/", 1)[0] not in top or "/../" in "/" + span + "/":
            return None
        return span.rstrip("/")

    named = []  # (file, line, what, the path or name it was read as, a bare name)
    for doc in sorted(f for f in ours if f.endswith(".md")):
        with open(doc, encoding="utf-8") as fh:
            text = fh.read()
        lines = text.split("\n")
        inside = False
        for i, line in enumerate(lines):
            if FENCE.match(line):
                inside = not inside
                lines[i] = ""
            elif inside:
                lines[i] = ""
        text = "\n".join(lines)
        spans = [m for p in PARAGRAPH.finditer(text) for m in CODE.finditer(text, p.start(), p.end())]
        for m in spans:
            path = a_path(m.group(2))
            if path:
                named.append((doc, line_of(text, m.start()), "`" + path + "`", path, False))
        for m in spans:
            text = text[:m.start()] + re.sub(r"[^\n]", " ", text[m.start():m.end()]) + text[m.end():]
        for m in list(LINK.finditer(text)) + list(REFERENCE.finditer(text)):
            target = m.group(1)
            if SCHEME.match(target) or target.startswith("#"):
                continue
            path = urllib.parse.unquote(re.split(r"[#?]", target, maxsplit=1)[0])
            if not path:
                continue
            path = posixpath.normpath(posixpath.join(posixpath.dirname(doc), path))
            named.append((doc, line_of(text, m.start(1)), "a link to " + target, path, False))

    for source in sorted(ours):
        if not os.path.isfile(source):
            continue
        try:
            with open(source, encoding="utf-8") as fh:
                text = fh.read()
        except UnicodeDecodeError:
            continue
        for at, comment in comments(source, text):
            for m in PATH.finditer(comment):
                path = a_path(m.group(1))
                if path:
                    named.append((source, line_of(text, at + m.start()), path, path, False))
            if not console:
                continue
            for m in BARE.finditer(comment):
                name = m.group(1)
                before = comment[:m.start()]
                if HISTORY.search(before) or before.endswith("/"):
                    continue
                named.append((source, line_of(text, at + m.start()), name, name, True))

    missing = [n for n in named if not (n[3] in names or n[3] in SKIP if n[4] else there(n[3]))]
    if missing:
        asked = sorted({n[3] for n in missing if not n[4]})
        ignored = subprocess.run(["git", "check-ignore", "--no-index", "--stdin"],
                                 input="\n".join(asked) + "\n", capture_output=True,
                                 text=True).stdout.split("\n")
        ignored = {p.rstrip("/") for p in ignored if p}
        missing = [n for n in missing if n[4] or n[3] not in ignored]

    until = os.environ.get("REPORT_ONLY_UNTIL", "")
    today = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")
    level = "warning" if until and today <= until else "error"
    on_github = bool(os.environ.get("GITHUB_ACTIONS"))
    for where, line, what, _, _ in missing:
        print(f"{where}:{line}: {what}, which is not there")
        if on_github:
            print(f"::{level} file={where},line={line}::{what}, which is not there. Name the file "
                  "where it is now, or see ci/check-named-files.py.")
    if not missing:
        print(f"Every file named is there ({len(named)} read).")
        return 0
    print(f"{len(missing)} named file(s) not there.")
    if level == "warning":
        print(f"Report only until {until} (UTC): passing. From the next day this fails.")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
