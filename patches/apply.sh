#!/bin/sh
# Reconstruct the isis_patches feature set on top of a clean master checkout.
#
# Usage: patches/apply.sh [name ...]
#
# Run from the repository root, on a clean checkout of master. With no
# arguments, applies every patches/*.patch file, in name-sorted order
# (feature names are numbered 01-, 02-, ... to control ordering). A subset
# may be given explicitly to build only some features, e.g.:
#   patches/apply.sh 01-infrastructure 02-alarm-messages
# (each feature requires 01-infrastructure to be applied first).
#
# A name's new files, if any, live in a same-named directory next to its
# .patch file (e.g. 01-infrastructure/ for 01-infrastructure.patch); a
# feature with no new files needs no such directory.

set -e

script_dir=$(cd "$(dirname "$0")" && pwd)
repo_root=$(pwd)

if [ "$#" -gt 0 ]; then
    names="$@"
else
    names=$(cd "$script_dir" && ls -- *.patch 2>/dev/null | sed 's/\.patch$//' | sort)
fi

if [ -z "$names" ]; then
    echo "apply.sh: no *.patch files found in $script_dir" >&2
    exit 1
fi

for name in $names; do
    patch="$script_dir/$name.patch"
    dir="$script_dir/$name"

    if [ ! -e "$patch" ]; then
        echo "apply.sh: no such patch file: $name.patch" >&2
        exit 1
    fi

    echo "== $name: copying files =="
    if [ -d "$dir" ]; then
        (cd "$dir" && find . -type f) | while read -r rel; do
            mkdir -p "$repo_root/$(dirname "$rel")"
            cp "$dir/$rel" "$repo_root/$rel"
            echo "  copied $rel"
        done
    else
        echo "  warning: no $name/ directory next to $name.patch; assuming no new files to copy" >&2
    fi

    echo "== $name: applying patch =="
    echo "  applying $name.patch"
    git apply "$patch"
done

echo "Done."
