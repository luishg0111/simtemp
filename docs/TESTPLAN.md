# Test Plan — nxp_simtemp Virtual Sensor Driver

**Component:** `nxp_simtemp` (misc device, sysfs + char dev)  
**Test scope:** Functional & behavioral verification of user–kernel interface  
**Tester:** Luis Hernández  
**Environment:** Ubuntu 24.04 LTS (x86_64 or arm64, kernel 6.x), user with sudo access  
**Device node:** `/dev/simtemp`  
**Sysfs path:** `/sys/class/simtemp/simtemp0`  
**Helper tools:**  
- `simtemp_ctl` (test binary helper)  
- `run_test.sh` (automated test runner)  
- `simtemp_cli.py` (interactive CLI for manual checks)

---
## Pre-Conditions for run test suite

| ID | Description |
|----|--------------|
| P1 | Kernel module `nxp_simtemp.ko` is built and available in `../kernel/build/`. |
| P2 | The user has root privileges (`sudo` access). |
| P3 | No instance of the module is currently loaded (`lsmod | grep nxp_simtemp` returns empty). |
| P4 | The helper binary `simtemp_ctl` is built and executable. |
| P5 | The directory `/sys/class/simtemp/simtemp0/` exists after module insertion. |
| P6 | Device node `/dev/simtemp` is created and readable. |

## Script for test suite
An script to automate the test suit is available in folder *test/*
run:
```
cd test
sudo ./run_test
```
## T1 — Load / Unload Validation

| Field | Details |
|--------|----------|
| **Objective** | Verify that the module loads and unloads cleanly, and creates all expected interfaces. |
| **Preconditions** | P1–P6 |
| **Steps** | 1. Run `sudo insmod nxp_simtemp.ko`.<br>2. Confirm device node `/dev/simtemp` and sysfs folder `/sys/class/simtemp/simtemp0/` exist.<br>3. List sysfs attributes: `sampling_ms`, `threshold_mc`, `mode`, `stats`.<br>4. Run `sudo rmmod nxp_simtemp`. |
| **Expected Result** | - Module loads without kernel warnings.<br>- All sysfs attributes are present.<br>- `dmesg` shows successful probe.<br>- After `rmmod`, sysfs and device nodes are removed.<br>- Return code = 0. |

---

## T2 — Periodic Sampling Rate

| Field | Details |
|--------|----------|
| **Objective** | Validate periodic temperature sampling frequency. |
| **Preconditions** | P1–P6 |
| **Steps** | 1. Insert the module.<br>2. Write `sampling_ms = 100` → `echo 100 | sudo tee /sys/class/simtemp/simtemp0/sampling_ms`.<br>3. Read samples for ~2 seconds using `simtemp_ctl rate 2`.<br>4. Compute observed sampling rate. |
| **Expected Result** | - Sampling frequency is within **10 ± 1 Hz** (9–11 Hz).<br>- No missed or corrupted samples.<br>- CLI live mode reflects new samples regularly. |

---

## T3 — Threshold Event & POLLPRI Signal

| Field | Details |
|--------|----------|
| **Objective** | Ensure the driver triggers `POLLPRI` when temperature crosses threshold. |
| **Preconditions** | P1–P6 |
| **Steps** | 1. Insert the module.<br>2. Set `sampling_ms = 100`.<br>3. Write a threshold slightly below average temp (≈ 36000 mC): `threshold_mc = 35900`.<br>4. Use `simtemp_ctl wait-threshold 300` to block for an alert event.<br>5. Observe `poll()` return flags. |
| **Expected Result** | - `poll()`/`select()` unblocks within 200–300 ms.<br>- Flag `POLLPRI` is raised.<br>- CLI live view shows `alert=1` for that sample.<br>- `stats` shows increment in `alerts_count`. |

---
## T4 — Error Paths & Robustness

| Field | Details |
|--------|----------|
| **Objective** | Validate driver’s resilience to invalid input and extreme sampling settings. |
| **Preconditions** | P1–P6 |
| **Steps** | 1. Insert the module.<br>2. Attempt to write invalid input: `echo abc > sampling_ms` (expect `EINVAL`).<br>3. Write `sampling_ms = 1` (fastest setting).<br>4. Observe that `updates_count` in `stats` continues increasing.<br>5. Confirm the system remains stable (no kernel warnings). |
| **Expected Result** | - Invalid writes return `-EINVAL` (or permission denied).<br>- Sampling continues without hangs.<br>- `stats/updates_count` increments.<br>- `alerts_count` and `errors_count` remain 0 unless threshold event occurs.<br>- No dmesg errors or oops. |

---
## T5 — Concurrency & Safe Access

| Field | Details |
|--------|----------|
| **Objective** | Verify concurrent access (reader + writer) doesn’t cause race conditions or crashes. |
| **Preconditions** | P1–P6 |
| **Steps** | 1. Insert the module.<br>2. Start a background reader: `simtemp_ctl read-dev 32 1000 &`.<br>3. In parallel, change configuration repeatedly:<br>  `sleep 0.05; echo 20, 50, 10, 100, 5, 40 > sampling_ms` sequentially.<br>4. Stop reader and check for errors. |
| **Expected Result** | - Both threads operate safely.<br>- No deadlocks or kernel warnings.<br>- `updates_count` increments steadily.<br>- Reader exits gracefully. |

---
# T6 — API Contract (Partial Read Support)

| Field | Details |
|--------|----------|
| **Objective** | Validate character device supports partial read requests gracefully. |
| **Preconditions** | P1–P6 |
| **Steps** | 1. Insert the module.<br>2. Perform partial read: `simtemp_ctl read-dev 7 3` (less than struct size).<br>3. Perform normal read: `simtemp_ctl read-dev 64 1` (exact struct size).<br>4. Check that both succeed. |
| **Expected Result** | - Partial reads return valid subset (≤ struct size).<br>- No blocking, corruption, or kernel crash.<br>- Full reads return expected binary sample.<br>- Device file correctly maintains offset / buffer boundaries. |

---
