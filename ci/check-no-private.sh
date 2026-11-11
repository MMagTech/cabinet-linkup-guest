#!/usr/bin/env bash
#
# No address, host name or home path of anybody's machines goes in a tracked file (CLAUDE.md).
# This repository is public, so this reads every tracked file for a private network address or a
# person's home folder, and fails on the first it finds. An address in an example is one of the
# documented ones: example.org, or 127.0.0.1.
#
#   ci/check-no-private.sh
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"
found=0
while IFS= read -r -d '' file; do
    [ -f "$file" ] || continue   # the submodule is a folder
    if ! perl -ne '
        BEGIN { $bad = 0 }
        if (/(?<![\w.])(?:10(?:\.\d{1,3}){3}|172\.(?:1[6-9]|2\d|3[01])(?:\.\d{1,3}){2}|192\.168(?:\.\d{1,3}){2}|100\.(?:6[4-9]|[7-9]\d|1[01]\d|12[0-7])(?:\.\d{1,3}){2})(?![\w]|\.\d)/
            || m{/(?:Users|home)/(?!runner\b)[A-Za-z]}
            || /[A-Za-z]:\\Users\\[A-Za-z]/) {
            print "$ARGV:$.: $_";
            $bad = 1;
        }
        END { exit $bad }
    ' "$file"; then
        found=1
    fi
done < <(git ls-files -z)

if [ "$found" -ne 0 ]; then
    echo "FAILED: a private address or a home folder's path is in a tracked file." >&2
    exit 1
fi
echo "ok: nothing private in a tracked file"
