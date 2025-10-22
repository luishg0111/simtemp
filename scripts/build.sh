#!/bin/bash
# ------------------------------------------------------------------------------
# build.sh - Build script for simtemp kernel module
# ------------------------------------------------------------------------------

# Exit immediately if a command exits with a non-zero status
set -e
# Exit when trying to use undefined variables
set -u
 
# Colors for status messages
PASS="\033[1;32m"   # PASS
ERR="\033[1;31m"    # ERR
WARN="\033[1;33m"   # WARN
NW="\033[0m"        # No Color

# Root of the repository (one level up from scripts/)
ROOT_DIR="$(dirname "$(realpath "$0")")/.."
KERNEL_DIR="${ROOT_DIR}/kernel"
BUILD_DIR="${KERNEL_DIR}/build"
KERNEL_RELEASE="$(uname -r)"
KERNEL_HEADERS="/lib/modules/${KERNEL_RELEASE}/build"

# Clean function
clean_build() {
    echo -e "${WARN}[*] Cleaning nxp_simtemp.ko build artifacts...${NW}"
    cd "${KERNEL_DIR}"
    make clean >/dev/null 2>&1 || true
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
        echo -e "${PASS}[OK] Build directory cleaned${NW}"
    fi
    exit 0
}

# Check if clean parameter was passed
if [ $# -eq 1 ] && [ "$1" = "clean" ]; then
    clean_build
elif [ $# -gt 0 ]; then
    echo -e "${ERR}[ERROR] Invalid argument: $1${NW}"
    echo "Usage: bash scripts/build.sh [clean]"
    exit 1
fi

echo -e "${WARN}[*] Building simtemp kernel module. Kernel version: ${KERNEL_RELEASE}${NW}"
# --------------------------------------------------------------------------
# Check dependeNWies
# --------------------------------------------------------------------------
if [ ! -d "${KERNEL_HEADERS}" ]; then
    echo -e "${ERR}[ERROR] Kernel headers not found at: ${KERNEL_HEADERS}${NW}"
    echo "Install with:"
    echo "  sudo apt install linux-headers-$(uname -r)"
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo -e "${ERR}[ERROR] 'make' command not found. Please install it.${NW}"
    echo "Install with:"
    echo "  sudo apt install build-essential"
    exit 1
fi

# --------------------------------------------------------------------------
# Build kernel module
# --------------------------------------------------------------------------
cd "${KERNEL_DIR}"
make clean >/dev/null 2>&1 || true
make

# --------------------------------------------------------------------------
# Verify and move build artifacts
# --------------------------------------------------------------------------
KO_FILE=$(find "${KERNEL_DIR}" -maxdepth 1 -name "*.ko" 2>/dev/null || true)

if [ -f "${KO_FILE}" ]; then
    echo -e "${PASS}[OK] Build successful!${NW}"

    echo -e  "${WARN}[*] Moving build artifacts ..."
    # Move build artifacts to build directory
    mkdir -p "${BUILD_DIR}"

    # Exclude the build output directory by name (we're running from ${KERNEL_DIR})
    BUILD_DIRNAME="$(basename "${BUILD_DIR}")"

    find . -type f \
        \( -name '*.ko' -o \
            -name '*.o' -o \
            -name '.*.cmd' -o \
            -name '*.o.cmd' -o \
            -name '*.mod' -o \
            -name '*.mod.c' -o \
            -name '*.d' -o \
            -name '*.symvers' -o \
            -name '*.order' \) \
        -not -path "./${BUILD_DIRNAME}/*" \
        -not -path "./.tmp_versions/*" \
        -not -path "./.git/*" \
        -exec mv -t "${BUILD_DIR}" -- {} +

    echo -e "-> Module built at: "${BUILD_DIR}"${NW}" 
else
    echo -e "${ERR}[ERROR] Build failed — module not found in ${BUILD_DIR}${NW}"
    exit 1
fi
# --------------------------------------------------------------------------
# Compile the Device Tree Overlay
# --------------------------------------------------------------------------
echo -e  "${WARN}[*] Compiling nxp-simtemp.dtsi ..."

if ! command -v dtc &> /dev/null; then
    echo -e "${ERR}[ERROR] 'make' command not found. Please install it.${NW}"
    echo "Install with:"
    echo "  sudo apt install device-tree-compiler"
    exit 1
fi

dtc -O dtb -o "${BUILD_DIR}/nxp-simtemp.dtbo" -@ \
    "${ROOT_DIR}/kernel/dts/nxp-simtemp.dtsi"
exit_code="$?"

if [ "${exit_code}" -ne 0 ] || [ ! -e "${BUILD_DIR}/nxp-simtemp.dtbo" ]; then
    echo -e  "${ERR}[ERROR] Device Tree Overlay compilation"
    local_exit 21
fi

echo -e  "${PASS}[OK] nxp-simtemp.dtbo created"

# --------------------------------------------------------------------------
# Success message
# --------------------------------------------------------------------------
echo -e "${PASS} Build Done. You can run:${NW}"
echo "  bash scripts/run_demo.sh"