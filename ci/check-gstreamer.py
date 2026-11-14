#!/usr/bin/env python3
"""
Is there a newer GStreamer with fixes than the one 2P Start is built with?

  python3 -I ci/check-gstreamer.py           the version in .github/workflows/build.yml
  python3 -I ci/check-gstreamer.py 1.28.7    another, to try the check

GStreamer's fixes, its security fixes among them, come out as newer versions in a series: 1.28.8,
then 1.28.9. This reads the version build.yml names for the Mac and Windows, which must be the same
(docs/decisions/2026-11-11-gstreamer-from-its-own-installer.md), and GStreamer's own download
folders for both. It exits 1, naming it, when a newer one in the same series is there for both; 0
when there is none; and 2 when it cannot tell. A newer one for only one system is said, and waits for
the other. A newer series is said too, and is a choice, not a fix.

It reads the packages, not GStreamer's tags: the tag comes days before the packages, and 2P Start
can move only once both systems have them. .github/workflows/gstreamer.yml runs it once a week
(docs/decisions/2026-11-13-hears-of-fixes.md).
"""
import pathlib
import re
import sys
import time
import urllib.request

SITE = "https://gstreamer.freedesktop.org/data/pkg"
FOLDERS = {"the Mac": "osx", "Windows": "windows"}
WORKFLOW = ".github/workflows/build.yml"
NOTES = "https://gstreamer.freedesktop.org/releases/{}.{}/"


def cannot_tell(why):
    print(f"Could not tell: {why}", file=sys.stderr)
    sys.exit(2)


def parse(text):
    return tuple(int(n) for n in text.split("."))


def named(version):
    return ".".join(str(n) for n in version)


def pinned():
    workflow = pathlib.Path(__file__).resolve().parent.parent / WORKFLOW
    found = set(re.findall(r"^\s*GST_VERSION:\s*(\d+\.\d+\.\d+)\s*$", workflow.read_text(), re.M))
    if len(found) != 1:
        cannot_tell(f"{WORKFLOW} should name one GStreamer version for the Mac and Windows, "
                    f"and names {', '.join(sorted(found)) or 'none'}")
    return found.pop()


def listed(folder):
    url = f"{SITE}/{folder}/"
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=60) as reply:
                page = reply.read().decode("utf-8", "replace")
            return {parse(v) for v in re.findall(r'href="(\d+\.\d+\.\d+)/"', page)}
        except OSError as error:
            if attempt == 2:
                cannot_tell(f"{url}: {error}")
            time.sleep(10)


def main():
    pin_text = sys.argv[1] if len(sys.argv) > 1 else pinned()
    if not re.fullmatch(r"\d+\.\d+\.\d+", pin_text):
        cannot_tell(f"{pin_text} is not a version such as 1.28.8")
    pin = parse(pin_text)
    series = pin[:2]
    found = {system: listed(folder) for system, folder in FOLDERS.items()}

    # The version 2P Start is built with must be there: if it is not, the page has changed and a
    # newer one would not be seen either.
    for system, versions in found.items():
        if pin not in versions:
            cannot_tell(f"{pin_text} is not in GStreamer's folder for {system}, "
                        f"{SITE}/{FOLDERS[system]}/, so the page may have changed")

    both = set.intersection(*found.values())
    fixes = sorted(v for v in both if v[:2] == series and v > pin)
    for system, versions in found.items():
        alone = sorted(v for v in versions - both if v[:2] == series and v > pin)
        if alone:
            print(f"GStreamer {named(alone[-1])} is out for {system} only, so it waits for the other.")
    # A stable series has an even second number; an odd one, such as 1.29, is the next in the making.
    later = sorted(v for v in both if v[1] % 2 == 0 and v[:2] > series)
    if later:
        print(f"GStreamer {named(later[-1])} is out, a newer series. Moving to it is a choice, not a fix.")

    if fixes:
        print(f"GStreamer {named(fixes[-1])} is out for the Mac and Windows, with fixes. "
              f"2P Start is built with {pin_text}.")
        print(f"What it fixes: {NOTES.format(*series)}")
        print(f"Move GST_VERSION and the three checksums beside it in {WORKFLOW}, "
              f"and every other place that names {pin_text} (git grep -F {pin_text}).")
        return 1
    print(f"ok: {pin_text} is the newest GStreamer {named(series)} for the Mac and Windows")
    return 0


if __name__ == "__main__":
    sys.exit(main())
