#!/usr/bin/env bash
# File: run_tests.sh
# Purpose: Execute T1..T6 for nxp_simtemp driver and show summary at the end.
# Notes:
#  - Runs all tests even if some fail
#  - Logs PASS/FAIL per test and a final summary
#  - Updated sysfs path: /sys/class/simtemp/simtemp0

set -u  # allow unset vars but keep safety (omit -e to not exit on errors)

# --- Module and paths ---
MOD_NAME="nxp_simtemp"
MOD_PATH="${MOD_PATH:-../kernel/build/${MOD_NAME}.ko}"
SYS="/sys/class/simtemp/simtemp0"
DEV="/dev/simtemp"
BIN="./simtemp_ctl"

# --- Internal counters ---
PASS_COUNT=0
FAIL_COUNT=0

log() { printf "[%s] %s\n" "$(date +'%F %T')" "$*"; }
pass() { echo "[OK] PASS: $*"; ((PASS_COUNT++)); }
fail() { echo "[X] FAIL: $*"; ((FAIL_COUNT++)); }

prep() {
  if [[ ! -x "$BIN" ]]; then
    log "Building helper..."
    make >/dev/null
  fi
  export SIMTEMP_SYSFS="$SYS"
  export SIMTEMP_DEV="$DEV"
}

# --- Simple assertions ---
assert_file() { [[ -e "$1" ]] || fail "Missing file: $1"; }
assert_contains() { grep -qE "$2" <<<"$1" || fail "Expected '$2' not found in output"; }

# --- T1 -------------------------------------------------------------
T1_load_unload() {
  log "T1 — Load/Unload"
  sudo rmmod "$MOD_NAME" 2>/dev/null || true
  sudo insmod "$MOD_PATH" || { fail "insmod failed"; return; }
  sleep 0.2

  assert_file "$DEV"
  assert_file "$SYS"
  for a in sampling_ms threshold_mc mode stats; do
    assert_file "$SYS/$a"
  done

  "$BIN" read-attr sampling_ms >/dev/null 2>&1 || fail "read sampling_ms"
  "$BIN" read-attr threshold_mc >/dev/null 2>&1 || fail "read threshold_mc"
  "$BIN" read-attr mode >/dev/null 2>&1 || fail "read mode"
  "$BIN" read-attr stats >/dev/null 2>&1 || fail "read stats"

  sudo rmmod "$MOD_NAME" || fail "rmmod failed"
  pass "T1"
}

# --- T2 -------------------------------------------------------------
T2_periodic_read() {
  log "T2 — Periodic Read (~10±1 Hz @ 100 ms)"
  sudo rmmod "$MOD_NAME" 2>/dev/null || true
  sudo insmod "$MOD_PATH" || { fail "insmod failed"; return; }

  "$BIN" write-attr sampling_ms 100 || { fail "set sampling_ms"; return; }
  sleep 0.1
  out=$("$BIN" rate 2 2>/dev/null) || { fail "rate measure"; return; }
  echo "$out"
  rate=$(awk -F'[ =]' '/RATE/{print $(NF-1)}' <<<"$out")
  if awk -v r="$rate" 'BEGIN{exit !(r>=9.0 && r<=11.5)}'; then
    pass "T2 ($rate Hz)"
  else
    fail "T2 rate out of range: $rate"
  fi
  sudo rmmod "$MOD_NAME" || true
}

# --- T3 -------------------------------------------------------------
T3_threshold_event() {
  log "T3 — Threshold Event (expect POLLPRI within ≤3 periods)"
  sudo insmod "$MOD_PATH" || { fail "insmod failed"; return; }
  "$BIN" write-attr sampling_ms 100 || { fail "set sampling_ms"; return; }

  mean_line=$("$BIN" mean 12 2>/dev/null) || { fail "mean sampling"; return; }
  echo "$mean_line"
  mean_mc=$(awk -F'[ =]' '/MEAN_mC/{print $2}' <<<"$mean_line")
  [[ -n "$mean_mc" ]] || { fail "No mean_mC parsed"; return; }

  thr=$(( mean_mc - 100 ))
  "$BIN" write-attr threshold_mc "$thr" || { fail "set threshold_mc"; return; }

  ev=$("$BIN" wait-threshold 300 2>/dev/null)
  echo "$ev"
  if grep -q "POLLPRI" <<<"$ev"; then
    pass "T3"
  else
    fail "POLLPRI not signaled"
  fi
  sudo rmmod "$MOD_NAME" || true
}

# --- T4 -------------------------------------------------------------
T4_error_paths() {
  log "T4 — Error Paths & Robustness"
  sudo insmod "$MOD_PATH" || { fail "insmod failed"; return; }

  if "$BIN" write-attr sampling_ms "abc" >/dev/null 2>&1; then
    fail "Expected invalid write to fail"
  fi

  "$BIN" write-attr sampling_ms 1 || fail "set sampling_ms=1"
  s1=$("$BIN" read-attr stats)
  sleep 0.2
  "$BIN" read-dev 32 3 >/dev/null 2>&1 || fail "read-dev at 1ms"
  s2=$("$BIN" read-attr stats)

  n1=$(grep -Eo '[0-9]+' <<<"$s1" | tail -1)
  n2=$(grep -Eo '[0-9]+' <<<"$s2" | tail -1)
  if [[ -n "$n1" && -n "$n2" && "$n2" -gt "$n1" ]]; then
    pass "T4"
  else
    fail "Stats did not increment"
  fi
  sudo rmmod "$MOD_NAME" || true
}

# --- T5 -------------------------------------------------------------
T5_concurrency() {
  log "T5 — Concurrency test"
  sudo insmod "$MOD_PATH" || { fail "insmod failed"; return; }

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
  sudo rmmod "$MOD_NAME" || fail "rmmod under concurrency"
  pass "T5"
}

# --- T6 -------------------------------------------------------------
T6_api_contract() {
  log "T6 — API Contract"
  sudo insmod "$MOD_PATH" || { fail "insmod failed"; return; }

  "$BIN" read-dev 7 3 >/dev/null 2>&1 || fail "partial read small size"
  "$BIN" read-dev 64 1 >/dev/null 2>&1 || fail "normal read"
  pass "T6"
  sudo rmmod "$MOD_NAME" || true
}

# --- Main -----------------------------------------------------------
main() {
  prep
  log "=== Starting simtemp test suite ==="

  T1_load_unload
  T2_periodic_read
  T3_threshold_event
  T4_error_paths
  T5_concurrency
  T6_api_contract

  log "=== Test summary ==="
  echo "  PASSED: $PASS_COUNT"
  echo "  FAILED: $FAIL_COUNT"

  if (( FAIL_COUNT > 0 )); then
    echo "[x] Some tests failed."
    exit 1
  else
    echo "[OK] All tests passed successfully."
  fi
}

main "$@"
