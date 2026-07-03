# Shared setup for patches/scripts/{generate,verify}.sh: resolves the
# script/patches/repo directories and reads patches/scripts/branches.conf.
# Source with:  . "$(dirname "$0")/patches_env.sh"

scripts_dir=$(cd "$(dirname "$0")" && pwd)
patches_dir=$(cd "$scripts_dir/.." && pwd)
repo_root=$(cd "$patches_dir/.." && pwd)
conf="$scripts_dir/branches.conf"

if [ ! -e "$conf" ]; then
    echo "$(basename "$0"): missing $conf" >&2
    exit 1
fi

# All non-comment, non-blank rows: "<name> <base-branch> <feature-branch>" per line.
all_rows=$(grep -Ev '^[[:space:]]*(#|$)' "$conf")
