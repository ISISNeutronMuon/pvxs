#!/bin/sh
# Reconstruct the isis_patches feature set on top of a clean master checkout.
#
# Usage: patches/apply.sh [dir ...]
#
# Run from the repository root, on a clean checkout of master. With no
# arguments, applies all feature directories in order:
#   01-infrastructure 02-alarm-messages 03-pvfilter
# A subset may be given explicitly to build only some features, e.g.:
#   patches/apply.sh 01-infrastructure 02-alarm-messages
# (each feature directory requires 01-infrastructure to be applied first).

set -e

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_root=$(pwd)

if [ "$#" -gt 0 ]; then
    dirs="$@"
else
    dirs="01-infrastructure 02-alarm-messages 03-pvfilter"
fi

for d in $dirs; do
    dir="$script_dir/$d"
    if [ ! -d "$dir" ]; then
        echo "apply.sh: no such patch directory: $d" >&2
        exit 1
    fi

    echo "== $d: copying files =="
    if [ -d "$dir/files" ]; then
        (cd "$dir/files" && find . -type f) | while read -r rel; do
            mkdir -p "$repo_root/$(dirname "$rel")"
            cp "$dir/files/$rel" "$repo_root/$rel"
            echo "  copied $rel"
        done
    fi

    echo "== $d: applying patches =="
    for patch in "$dir"/*.patch; do
        [ -e "$patch" ] || continue
        echo "  applying $(basename "$patch")"
        git apply "$patch"
    done
done

echo "Done."
