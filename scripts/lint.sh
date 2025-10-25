# ...existing code...
#!/bin/bash
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../kernel" && pwd)"
MODULE_SRC="$PROJECT_ROOT/src"

echo -e $SCRIPT_DIR¨ ¨$PROJECT_ROOT
# Required external tools
REQUIRED=(perl find uname)
MISSING=()
for cmd in "${REQUIRED[@]}"; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        MISSING+=("$cmd")
    fi
done

if [ "${#MISSING[@]}" -ne 0 ]; then
    echo "warning: missing required tools: ${MISSING[*]}"
    echo "Install the missing packages (sudo apt install perl findutils coreutils) and re-run."
    exit 2
fi

# Prefer the kernel build symlink (if kernel headers/sources are installed this usually points to the correct source tree)
MOD_BUILD="/lib/modules/$(uname -r)/build/scripts/checkpatch.pl"

KERNEL=""
if [ -f "$MOD_BUILD" ] && [ -r "$MOD_BUILD" ]; then
    KERNEL="$MOD_BUILD"
else
    # Search /usr/src for linux-* trees and try to pick best match
    UNAME_BASE="$(uname -r)"
    UNAME_PREFIX="${UNAME_BASE%%-*}"   # drop distro suffix like "-generic"
    for d in /usr/src/linux-*; do
        [ -d "$d" ] || continue
        cand="$d/scripts/checkpatch.pl"
        if [ -f "$cand" ] && [ -r "$cand" ]; then
            # prefer a directory that contains the uname prefix
            if printf '%s\n' "$d" | grep -q "$UNAME_PREFIX"; then
                KERNEL="$cand"
                break
            fi
            # keep first found as fallback
            [ -z "$KERNEL" ] && KERNEL="$cand"
        fi
    done
fi

PATH_CHECKPATCH="$(command -v checkpatch.pl 2>/dev/null || true)"

# Decide which checkpatch to use.
CHECKPATCH=""
if [ -n "$KERNEL" ] && [ -f "$KERNEL" ] && [ -r "$KERNEL" ]; then
    CHECKPATCH="$KERNEL"
elif [ -n "$PATH_CHECKPATCH" ]; then
    CHECKPATCH="$PATH_CHECKPATCH"
else
    echo "warning: checkpatch.pl not found in any candidate locations:"
    echo "  - /lib/modules/$(uname -r)/build/scripts/checkpatch.pl"
    echo "  - /usr/src/linux-*/scripts/checkpatch.pl"
    echo "  - PATH (checkpatch.pl)"
    echo "Install kernel headers/sources or add checkpatch.pl to PATH."
    exit 2
fi

# Options to pass to checkpatch; modify or allow passing extra args if desired
CP_ARGS=(--terse --strict --no-tree)

FAIL=0
LOG_FILE="$SCRIPT_DIR/lint_report.log"
ERROR_COUNT=0
WARNING_COUNT=0

# Find .c files excluding .git and run checkpatch on each
{
    while IFS= read -r -d '' file; do
        printf '\n*** checkpatch: %s\n' "$file"

        # Use -f option to check individual files and append output to lint report
        perl "$CHECKPATCH" -f "$file" "${CP_ARGS[@]}" | tee -a "$LOG_FILE"
    done < <(find "$MODULE_SRC" -type f \( -name '*.c' \) -not -path '*/.git/*' -print0)
} > >(sed 's/^/\033[31m&\033[0m/')  # Colorize output

# Count errors and warnings from the uncolored log
ERROR_COUNT=$(grep -E ':[0-9]+: ERROR:' "$LOG_FILE" | wc -l || true)
WARNING_COUNT=$(grep -E ':[0-9]+: WARNING:' "$LOG_FILE" | wc -l || true)


# Print totals (modified lines requested)
echo "Total Errors: $ERROR_COUNT"
echo "Total Warnings: $WARNING_COUNT"

if [ "${ERROR_COUNT:-0}" -ne 0 ]; then
    echo
    echo "checkpatch found errors. Fix them before committing."
    exit 1
fi

echo "checkpatch: no errors found in .c files"
exit 0