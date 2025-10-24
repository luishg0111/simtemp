#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# File: run_demo.sh
# Purpose: Reload nxp_simtemp driver and launch two CLI consoles for demo
# Author: Luis Hernández
# Location: scripts/
# ---------------------------------------------------------------------------

set -euo pipefail

# --- Configuration ---------------------------------------------------------
MOD_NAME="nxp_simtemp"
MOD_PATH="../kernel/build/${MOD_NAME}.ko"
CLI_PATH="../user/cli/simtemp_cli.py"

# --- Helper functions ------------------------------------------------------
log() { echo "[INFO] $*"; }
warn() { echo -e "\033[93m[WARN]\033[0m $*"; }
error() { echo -e "\033[91m[ERR]\033[0m $*" >&2; exit 1; }

# --- Check dependencies ----------------------------------------------------
command -v gnome-terminal >/dev/null 2>&1 || \
  command -v konsole >/dev/null 2>&1 || \
  command -v xterm >/dev/null 2>&1 || \
  error "No compatible terminal found (requires gnome-terminal, konsole, or xterm)."

if [[ ! -f "$MOD_PATH" ]]; then
  error "Kernel module not found: $MOD_PATH"
fi

if [[ ! -f "$CLI_PATH" ]]; then
  error "CLI not found: $CLI_PATH"
fi

# --- Module handling -----------------------------------------------------
if lsmod | grep "^${MOD_NAME}\b"; then
  warn "Module ${MOD_NAME} is already loaded. Removing..."
  sudo rmmod "$MOD_NAME" || error "Failed to remove module."
  sleep 0.5
fi

log "Inserting kernel module..."
sudo insmod "$MOD_PATH" || error "Failed to insert module."
sleep 0.3

if ! lsmod | grep "^${MOD_NAME}\b"; then
  error "Module failed to load."
fi

log "Module ${MOD_NAME} successfully loaded."

# --- Launch CLI consoles ---------------------------------------------------
log "Launching CLI consoles..."
if command -v gnome-terminal >/dev/null 2>&1; then
  # First terminal: interactive CLI
  gnome-terminal -- bash -c "cd ../user/cli && sudo ./simtemp_cli.py; exec bash" &
  # Second terminal: auto-start in live view (v)
  gnome-terminal -- bash -c "cd ../user/cli && sudo ./simtemp_cli.py v; exec bash" &

elif command -v konsole >/dev/null 2>&1; then
  konsole --new-tab -e bash -c "cd ../user/cli && sudo ./simtemp_cli.py" &
  konsole --new-tab -e bash -c "cd ../user/cli && sudo ./simtemp_cli.py v" &

elif command -v xterm >/dev/null 2>&1; then
  xterm -hold -e "cd ../user/cli && sudo ./simtemp_cli.py" &
  xterm -hold -e "cd ../user/cli && sudo ./simtemp_cli.py v" &
else
  error "Could not open terminals. Install gnome-terminal, konsole or xterm."
fi

log "Demo environment started.
- Window 1: interactive CLI
- Window 2: live streaming view (v)"
log "Press Ctrl+C to stop the demo."
