#!/bin/sh
# Regenerate patches/<name>.patch and patches/<name>/ straight from git,
# by diffing the two branches named for <name> in
# patches/scripts/branches.conf.
#
# Usage: patches/scripts/generate.sh [name ...]
#
# With no arguments, regenerates every patch listed in branches.conf. Never
# hand-edit a patches/*.patch file or the files under patches/<name>/ --
# edit the real source on the feature branch instead, commit, then rerun
# this script.

set -e

. "$(dirname "$0")/patches_env.sh"

# Read the requested (or all) rows up front, so an unknown name is reported
# before any work is done.
rows="$all_rows"
if [ "$#" -gt 0 ]; then
    wanted="$*"
    filtered=""
    for name in $wanted; do
        row=$(echo "$rows" | awk -v n="$name" '$1==n')
        if [ -z "$row" ]; then
            echo "generate.sh: no such patch in $conf: $name" >&2
            exit 1
        fi
        filtered="$filtered
$row"
    done
    rows="$filtered"
fi

cd "$repo_root"

echo "$rows" | while IFS= read -r row; do
    [ -z "$row" ] && continue
    set -- $row
    name="$1"; base="$2"; feature="$3"

    for ref in "$base" "$feature"; do
        if ! git rev-parse --verify --quiet "$ref^{commit}" >/dev/null; then
            echo "generate.sh: $name: no such branch/ref: $ref" >&2
            exit 1
        fi
    done

    echo "== $name: diffing $base..$feature =="

    # One status pass covers rename/copy rejection, new files to copy
    # verbatim, and modified/deleted files to diff.
    status=$(git diff --name-status "$base" "$feature")
    unexpected=$(echo "$status" | awk '$1 ~ /^[RC]/')
    if [ -n "$unexpected" ]; then
        echo "generate.sh: $name: rename/copy changes aren't supported by this script:" >&2
        echo "$unexpected" >&2
        exit 1
    fi

    dir="$patches_dir/$name"
    added=$(echo "$status" | awk '$1 == "A" {print $2}')
    rm -rf "$dir"
    if [ -n "$added" ]; then
        mkdir -p "$dir"
        echo "$added" | while IFS= read -r rel; do
            [ -z "$rel" ] && continue
            mkdir -p "$dir/$(dirname "$rel")"
            git show "$feature:$rel" > "$dir/$rel"
            echo "  new file: $rel"
        done
    fi

    patch="$patches_dir/$name.patch"
    changed=$(echo "$status" | awk '$1 == "M" || $1 == "D" {print $2}')
    if [ -n "$changed" ]; then
        # Pure-addition hunks have their leading context dropped: two
        # sibling patches that each append to the same shared file (e.g.
        # ioc/Makefile's SRCS list, setup.py's DSOS, sitehooks.cpp's
        # registerHooks()) both anchor by default on the line *before* the
        # insertion -- the same anchor -- so applying both in sequence
        # conflicts even though neither touches a line the other changed.
        # Dropping the leading context anchors the hunk on what follows
        # instead, which no sibling contests. This is a no-op for files no
        # other patch touches, so it's applied to every diff rather than
        # only to files some other patch happens to also change -- which
        # patches end up stacked together is a fact about the combination
        # model (see verify.sh), not about any one file. See
        # trim_leading_context.py for the full explanation.
        git diff --no-color "$base" "$feature" -- $changed \
            | python3 "$scripts_dir/trim_leading_context.py" > "$patch"
        echo "  wrote $(basename "$patch")"
    else
        rm -f "$patch"
        echo "  no modified files; removed $(basename "$patch") if present"
    fi
done

echo "Done."
