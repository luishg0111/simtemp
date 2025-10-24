#!/bin/bash
# ------------------------------------------------------------------------------
# build.sh - Build script for simtemp kernel module
# ------------------------------------------------------------------------------
# Exit immediately if a command exits with a non-zero status
set -e
# Exit when trying to use undefined variables
set -u

# --------------------------------------------------------------------------
# Colors for status messages
# --------------------------------------------------------------------------
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
RED="\033[1;31m"
NC="\033[0m"

# --------------------------------------------------------------------------
# Root of the repository (one level up from scripts/)
# --------------------------------------------------------------------------
ROOT_DIR="$(dirname "$(realpath "$0")")/.."
KERNEL_DIR="${ROOT_DIR}/kernel"
BUILD_DIR="${KERNEL_DIR}/build"
KERNEL_RELEASE="$(uname -r)"
KERNEL_HEADERS="/lib/modules/${KERNEL_RELEASE}/build"

# --------------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------------
TARGET=${1:-host}
DEBUG_FLAG=${DEBUG:-0}
# --------------------------------------------------------------------------
# Toolchains
# --------------------------------------------------------------------------
RPI_TOOLCHAIN="/usr/bin/arm-linux-gnueabihf-"
RPI_ARCH="arm"
RPI_KERNEL_DIR="${ROOT_DIR}/rpi-kernel-headers"

# --------------------------------------------------------------------------
# Helper functions
# --------------------------------------------------------------------------
info()  { echo -e "${GREEN}[OK] $1${NC}"; }
warn()  { echo -e "${YELLOW}[!] $1${NC}"; }
error() { echo -e "${RED}[x] $1${NC}"; exit 1; }

# --------------------------------------------------------------------------
# Detection helpers
# --------------------------------------------------------------------------
is_qemu_arm() {
	# Detect if we're inside a QEMU ARM virtualized environment
	if uname -m | grep -qi "aarch64"; then
		return 0
	else
		return 1
	fi
}

# --------------------------------------------------------------------------
# Check dependencies
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
# Build actions
# --------------------------------------------------------------------------
build_host() {
	info "Building for host (native)..."
	make -C "${KERNEL_DIR}" all
	make -C "${KERNEL_DIR}" dtbo
}

build_rpi() {
	info "Cross-compiling for Raspberry Pi 3B+ (ARMv7)..."

	if [ ! -x "$(command -v ${RPI_TOOLCHAIN}gcc)" ]; then
		echo -e "${RED}[!] Toolchain not found: ${RPI_TOOLCHAIN}gcc${RESET}"
		echo "    Install it with: sudo apt install gcc-arm-linux-gnueabihf"
		exit 1
	fi

	# Choose kernel headers
	if [ -d "${RPI_KERNEL_DIR}" ]; then
		KDIR="${RPI_KERNEL_DIR}"
	else
		warn "No local Pi headers found, using host headers for symbol compatibility check."
		KDIR="/lib/modules/$(uname -r)/build"
	fi

	make -C "${KDIR}" M="${KERNEL_DIR}" ARCH=${RPI_ARCH} CROSS_COMPILE=${RPI_TOOLCHAIN} DEBUG=$(DEBUG_FLAG) modules
	make -C "${KERNEL_DIR}" dtbo
}

build_qemu() {
	info "Building for QEMU ARM (aarch64)..."
	make -C "${KERNEL_DIR}" ARCH=arm64 all
	make -C "${KERNEL_DIR}" dtbo
}

clean_build() {
	warn "Cleaning build artifacts..."
	make -C "${KERNEL_DIR}" clean || true
	rm -rf "${BUILD_DIR}"
}

# --------------------------------------------------------------------------
# Dispatcher
# --------------------------------------------------------------------------
case "${TARGET}" in
	host|"")
		build_host
		;;
	rbpi|rpi|pi)
		build_rpi
		;;
	qemu)
		if is_qemu_arm; then
			build_qemu
		else
			warn "Not running under ARM (QEMU). Forcing native build."
			build_host
		fi
		;;
	clean)
		clean_build
		;;
	*)
		error "Unknown target '${TARGET}'. Usage: $0 [host|rbpi|qemu|clean]"
		;;
esac

# --------------------------------------------------------------------------
# Success message
# --------------------------------------------------------------------------
echo -e "${PASS} Build complete. You can try :${NW}"
echo "  bash scripts/run_demo.sh"