#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# File: run_test.sh
# Purpose: Automated test suite for nxp_simtemp driver (T1..T6)
# Author: Luis Hernández
# ---------------------------------------------------------------------------

set -u  # Do not exit on first error; we want to run all tests

# --- Configuration ---------------------------------------------------------
MOD_NAME="nxp_simtemp"
MOD_PATH="${MOD_PATH:-../kernel/build/${MOD_NAME}.ko}"
SYSFS_BASE="/sys/class/simtemp/simtemp0"
DEV_PATH="/dev/simtemp"
BIN="./simtemp_ctl"

# --- Colors ----------------------------------------------------------------
C_GREEN="\033[92m"
C_YELLOW="\033[93m"
C_RED="\033[91m"
C_CYAN="\033[96m"
C_RESET="\033[0m"
C_BOLD="\033[1m"

# --- Result counters -------------------------------------------------------
PASS_COUNT=0
FAIL_COUNT=0

# --- Helper functions ------------------------------------------------------
log()   { echo -e "${C_CYAN}[INFO]${C_RESET} $*"; }
ok()    { echo -e "${C_GREEN}[OK]${C_RESET} $*"; ((PASS_COUNT++)); }
warn()  { echo -e "${C_YELLOW}[WARN]${C_RESET} $*"; }
fail()  { echo -e "${C_RED}[ERR]${C_RESET} $*"; ((FAIL_COUNT++)); }
line()  { echo "--------------------------------------------------------"; }

assert_file() {
  [[ -e "$1" ]] || { fail "Missing file: $1"; return 1; }
}

# --- Module management -----------------------------------------------------
reload_module() {
  if lsmod | grep -q "^${MOD_NAME}\b"; then
    warn "Module already loaded. Removing..."
    sudo rmmod "$MOD_NAME" 2>/dev/null || fail "Failed to remove module"
    sleep 0.3
  fi
  log "Loading kernel module..."
  sudo insmod "$MOD_PATH" || fail "insmod failed"
  sleep 0.2
  if ! lsmod | grep -q "^${MOD_NAME}\b"; then
    fail "Module not loaded after insmod"
  else
    ok "Module loaded successfully"
  fi
}

cleanup_module() {
  if lsmod | grep -q "^${MOD_NAME}\b"; then
    sudo rmmod "$MOD_NAME" 2>/dev/null || warn "Failed to remove module"
  fi
}

# --- Preparation -----------------------------------------------------------
prep() {
  [[ -x "$BIN" ]] || { log "Building helper..."; make >/dev/null || fail "Build failed"; }
  export SIMTEMP_SYSFS="$SYSFS_BASE"
  export SIMTEMP_DEV="$DEV_PATH"
}

# --- Helper: parse updates_count ------------------------------------------
parse_updates_count() {
  awk '
    BEGIN{val=""}
    /updates_count/ {
      for(i=1;i<=NF;i++){
        if ($i ~ /updates_count/){
          gsub(/updates_count[=:]/,"",$i);
          gsub(/[^0-9]/,"",$i);
          if ($i!=""){val=$i; break}
        }
      }
    }
    END{if(val!="") print val}
  '
}

# ---------------------------------------------------------------------------
# Test Cases
# ---------------------------------------------------------------------------

T1_load_unload() {
  line
  log "T1 — Load/Unload"
  reload_module
  assert_file "$DEV_PATH"
  assert_file "$SYSFS_BASE"
  for a in sampling_ms threshold_mc mode stats; do
    assert_file "$SYSFS_BASE/$a"
  done
  "$BIN" read-attr sampling_ms >/dev/null || fail "Read sampling_ms"
  cleanup_module
  ok "T1 completed"
}

T2_periodic_read() {
  line
  log "T2 — Periodic Read (~10±1 Hz @ 100 ms)"
  reload_module
  "$BIN" write-attr sampling_ms 100 || fail "Set sampling_ms"
  sleep 0.1
  rate_output=$("$BIN" rate 2 2>/dev/null) || fail "rate() failed"
  echo "$rate_output"
  rate=$(awk -F'[ =]' '/RATE/{print $(NF-1)}' <<<"$rate_output")
  if awk -v r="$rate" 'BEGIN{exit !(r>=9.0 && r<=11.5)}'; then
    ok "T2 rate OK (${rate}Hz)"
  else
    fail "T2 rate out of range: ${rate}Hz"
  fi
  cleanup_module
}

T3_threshold_event() {
  line
  log "T3 — Threshold Event (expect POLLPRI when threshold < 36000 mC)"
  reload_module
  "$BIN" write-attr sampling_ms 100 || fail "Set sampling_ms"
  TARGET_THR=35900
  "$BIN" write-attr threshold_mc "$TARGET_THR" || { fail "Set threshold_mc"; cleanup_module; return; }
  ev=$("$BIN" wait-threshold 300 2>/dev/null || true)
  echo "$ev"
  if grep -q "POLLPRI" <<<"$ev"; then
    ok "T3 POLLPRI received (threshold=${TARGET_THR})"
  else
    fail "T3 POLLPRI not signaled (threshold=${TARGET_THR})"
  fi
  cleanup_module
}

T4_error_paths() {
  line
  log "T4 — Error Paths & Robustness"
  reload_module
  if "$BIN" write-attr sampling_ms abc >/dev/null 2>&1; then
    fail "Expected invalid write to fail"
  fi
  "$BIN" write-attr sampling_ms 1 || fail "Set sampling_ms=1"
  s1=$("$BIN" read-attr stats)
  sleep 0.2
  "$BIN" read-dev 32 3 >/dev/null || fail "read-dev failed"
  s2=$("$BIN" read-attr stats)
  n1=$(grep -Eo '[0-9]+' <<<"$s1" | tail -1)
  n2=$(grep -Eo '[0-9]+' <<<"$s2" | tail -1)
  if [[ -n "$n1" && -n "$n2" && "$n2" -gt "$n1" ]]; then
    ok "T4 stats incremented"
  else
    fail "T4 stats did not increment"
  fi
  cleanup_module
}

T5_concurrency() {
  line
  log "T5 — Concurrency test"
  reload_module
  "$BIN" write-attr sampling_ms 20 || fail "sampling_ms=20"
  "$BIN" read-dev 32 1000 >/tmp/simtemp_reader.log 2>&1 &
  READER_PID=$!
  sleep 0.2
  for ms in 20 50 10 100 5 40; do
    "$BIN" write-attr sampling_ms "$ms" || fail "write sampling_ms=$ms"
    sleep 0.05
  done
  if ! kill -0 "$READER_PID" 2>/dev/null; then
    fail "reader crashed"
  fi
  kill "$READER_PID" 2>/dev/null || true
  wait "$READER_PID" 2>/dev/null || true
  cleanup_module
  ok "T5 completed"
}

T6_api_contract() {
  line
  log "T6 — API Contract (partial reads)"
  reload_module
  # Partial read smaller than struct size
  if "$BIN" read-dev 7 3 >/dev/null 2>&1; then
    ok "T6 partial read accepted (size<struct)"
  else
    fail "T6 partial read failed unexpectedly"
  fi
  # Full-size read should still work
  if "$BIN" read-dev 64 1 >/dev/null 2>&1; then
    ok "T6 full read OK"
  else
    fail "T6 full read failed"
  fi
  cleanup_module
}

# ---------------------------------------------------------------------------
# Main Execution
# ---------------------------------------------------------------------------
main() {
  prep
  echo -e "${C_BOLD}Starting simtemp automated test suite${C_RESET}"
  line

  T1_load_unload
  T2_periodic_read
  T3_threshold_event
  T4_error_paths
  T5_concurrency
  T6_api_contract

  line
  echo -e "${C_BOLD}Test summary${C_RESET}"
  echo -e "  ${C_GREEN}PASSED:${C_RESET} $PASS_COUNT"
  echo -e "  ${C_RED}FAILED:${C_RESET} $FAIL_COUNT"
  line

  if (( FAIL_COUNT > 0 )); then
    warn "Some tests failed."
    exit 1
  else
    ok "All tests passed successfully."
  fi
}

main "$@"
