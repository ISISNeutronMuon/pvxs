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

scripts_dir=$(cd "$(dirname "$0")" && pwd)
patches_dir=$(cd "$scripts_dir/.." && pwd)
repo_root=$(cd "$patches_dir/.." && pwd)
conf="$scripts_dir/branches.conf"

if [ ! -e "$conf" ]; then
    echo "generate.sh: missing $conf" >&2
    exit 1
fi

# Read the requested (or all) rows from branches.conf up front, so an
# unknown name is reported before any work is done. all_rows is kept around
# (even when a subset is requested) to find sibling patches sharing a base
# below.
all_rows=$(grep -v '^[[:space:]]*#' "$conf" | grep -v '^[[:space:]]*$')
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

    unexpected=$(git diff --diff-filter=CR --name-only "$base" "$feature")
    if [ -n "$unexpected" ]; then
        echo "generate.sh: $name: rename/copy changes aren't supported by this script:" >&2
        echo "$unexpected" >&2
        exit 1
    fi

    dir="$patches_dir/$name"
    added=$(git diff --diff-filter=A --name-only "$base" "$feature")
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

    # Files also touched (added or modified) by a sibling patch stacked on
    # the same base: two siblings independently appending to the same list
    # (ioc/Makefile's SRCS, setup.py's DSOS, sitehooks.cpp's registerHooks())
    # insert at the same conceptual spot, so a default-context diff for
    # either one anchors on the *other's* insertion point too and the two
    # conflict when applied in sequence. Route these through
    # trim_leading_context.py, which drops the leading (before-insertion)
    # context from pure-addition hunks so each sibling's hunk is instead
    # anchored only by what follows it -- unaffected by what a sibling
    # inserted earlier at the same spot. See that script for the full
    # explanation.
    sibling_files=$(echo "$all_rows" | while IFS= read -r other; do
        [ -z "$other" ] && continue
        set -- $other
        oname="$1"; obase="$2"; ofeature="$3"
        [ "$oname" = "$name" ] && continue
        [ "$obase" = "$base" ] || continue
        git diff --diff-filter=AM --name-only "$obase" "$ofeature"
    done)

    patch="$patches_dir/$name.patch"
    changed=$(git diff --diff-filter=MD --name-only "$base" "$feature")
    if [ -n "$changed" ]; then
        : > "$patch"
        echo "$changed" | while IFS= read -r f; do
            [ -z "$f" ] && continue
            shared=0
            for sf in $sibling_files; do
                [ "$sf" = "$f" ] && shared=1 && break
            done
            if [ "$shared" = "1" ]; then
                git diff --no-color "$base" "$feature" -- "$f" \
                    | python3 "$scripts_dir/trim_leading_context.py" >> "$patch"
                echo "  wrote hunk for $f (leading context trimmed: shared with a sibling patch)"
            else
                git diff --no-color "$base" "$feature" -- "$f" >> "$patch"
                echo "  wrote hunk for $f"
            fi
        done
        echo "  wrote $(basename "$patch")"
    else
        rm -f "$patch"
        echo "  no modified files; removed $(basename "$patch") if present"
    fi
done

echo "Done."
