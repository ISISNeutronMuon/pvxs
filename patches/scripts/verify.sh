#!/bin/sh
# Confirm patches/*.patch actually apply -- and, with --full, actually
# build and pass tests -- for every combination CLAUDE.md requires to stay
# buildable: 01-infrastructure alone, 01+02, 01+03, and all patches
# together. Run this (with --full before pushing) after
# patches/scripts/generate.sh whenever a patch-NN-* branch changes.
#
# Usage: patches/scripts/verify.sh [--full] [name ...]
#
# With no names, checks every patch listed in branches.conf. Named patches
# are always verified stacked on 01-infrastructure (as apply.sh requires),
# plus the combination of everything together.

set -e

full=0
if [ "$1" = "--full" ]; then
    full=1
    shift
fi

scripts_dir=$(cd "$(dirname "$0")" && pwd)
patches_dir=$(cd "$scripts_dir/.." && pwd)
repo_root=$(cd "$patches_dir/.." && pwd)
conf="$scripts_dir/branches.conf"

all_names=$(grep -v '^[[:space:]]*#' "$conf" | grep -v '^[[:space:]]*$' | awk '{print $1}')
if [ "$#" -gt 0 ]; then
    names="$*"
else
    names=$(echo "$all_names" | tr '\n' ' ')
fi

# 01-infrastructure is implied by every combination; build the list of
# combos to check: [01], [01 X] for each other named patch, and [01 <all
# others>] together.
first=$(echo "$all_names" | head -n1)
others=$(echo "$all_names" | tail -n +2)

combos="$first"
for n in $others; do
    case " $names " in
        *" $n "*) combos="$combos
$first $n" ;;
    esac
done
all_others=$(echo "$others" | tr '\n' ' ' | sed 's/ *$//')
if [ -n "$all_others" ]; then
    combos="$combos
$first $all_others"
fi

run_combo() {
    combo="$1"
    label=$(echo "$combo" | tr ' ' '+')
    wt="$repo_root/../pvxs-verify-$label.$$"
    echo "== verifying: $label =="

    git -C "$repo_root" worktree add -q --detach "$wt" master
    ( cd "$wt" && sh "$scripts_dir/apply.sh" $combo )

    if [ "$full" = "1" ]; then
        # Reuse the EPICS Base this repo is already configured to build
        # against (configure/RELEASE.local) rather than fetching/building a
        # separate copy -- this repo already builds successfully with it.
        if [ ! -e "$repo_root/configure/RELEASE.local" ]; then
            echo "verify.sh --full: $repo_root/configure/RELEASE.local not found;" >&2
            echo "  set EPICS_BASE there first (see configure/RELEASE for the format)." >&2
            exit 1
        fi
        cp "$repo_root/configure/RELEASE.local" "$wt/configure/RELEASE.local"
        (
            cd "$wt"
            make -j"$(nproc)"
            make -s test-results
        )
    fi

    git -C "$repo_root" worktree remove --force "$wt"
    echo "== $label: OK =="
}

echo "$combos" | while IFS= read -r combo; do
    [ -z "$combo" ] && continue
    run_combo "$combo"
done

echo "All combinations verified."
