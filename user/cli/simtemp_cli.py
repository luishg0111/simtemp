#!/usr/bin/env python3
"""
simtemp_cli.py
--------------
CLI interface to interact with the nxp_simtemp kernel driver.
Provides commands to read/write sysfs attributes and stream temperature samples.
"""

import os
import struct
import datetime
import select
import sys
from pathlib import Path
from collections import deque

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
SYSFS_BASE = Path("/sys/class/simtemp/simtemp0")
DEV_PATH = Path("/dev/simtemp")
SAMPLE_STRUCT_FMT = "<Q i I"  # little-endian: u64 timestamp, s32 temp_mc, u32 flags
SAMPLE_SIZE = struct.calcsize(SAMPLE_STRUCT_FMT)

MAX_DISPLAY_SAMPLES = 25  # number of visible samples

# Defines for input validation
VALID_MODES = ["normal", "noisy", "ramp"]
MIN_SAMPLING_MS = 1
MAX_SAMPLING_MS = 10000
MIN_THRESHOLD_MC = -5000
MAX_THRESHOLD_MC = 100000

# ---------------------------------------------------------------------------
# ANSI Colors
# ---------------------------------------------------------------------------
class C:
	GREEN = "\033[92m"
	YELLOW = "\033[93m"
	RED = "\033[91m"
	CYAN = "\033[96m"
	RESET = "\033[0m"
	BOLD = "\033[1m"

# ---------------------------------------------------------------------------
# Sysfs helpers
# ---------------------------------------------------------------------------
def read_attr(name: str) -> str:
	"""@brief Read a sysfs attribute and return its content as string."""
	path = SYSFS_BASE / name
	with open(path, "r") as f:
		return f.read().strip()

def write_attr(name: str, value: str) -> None:
	"""@brief Write a value to a sysfs attribute."""
	path = SYSFS_BASE / name
	with open(path, "w") as f:
		f.write(str(value))

def list_all_attrs() -> None:
	"""@brief Display all sysfs attributes and values."""
	attrs = ["sampling_ms", "threshold_mc", "mode", "stats"]
	print(f"{C.CYAN}{C.BOLD}Current attributes:{C.RESET}")
	for a in attrs:
		try:
			val = read_attr(a)
			print(f"  {a:15s}: {val}")
		except Exception as e:
			print(f"  {a:15s}: {C.YELLOW}[!] Error: {e}{C.RESET}")

# ---------------------------------------------------------------------------
# Sample decoding and streaming
# ---------------------------------------------------------------------------
def decode_sample(data: bytes, alert: int) -> str:
	"""@brief Decode 16-byte sample struct into readable text + alert flag."""
	if len(data) < SAMPLE_SIZE:
		return "[incomplete sample]"
	ts_ns, temp_mc, flags = struct.unpack(SAMPLE_STRUCT_FMT, data)
	dt = datetime.datetime.utcfromtimestamp(ts_ns / 1e9).isoformat(timespec="milliseconds") + "Z"
	temp_c = temp_mc / 1000.0
	alert_text = f"{C.GREEN}0{C.RESET}" if alert == 0 else f"{C.RED}1{C.RESET}"
	return f"{dt}   Temperature={temp_c:5.1f}C  Threshold alert={alert_text}"

def live_stream(max_samples: int = 10) -> None:
	"""@brief Live mode: reads samples and refreshes screen with limited window."""
	samples = deque(maxlen=max_samples)
	count = 0
	os.system("clear")
	print("Sensor information (live view)")
	print("Press Ctrl+C to stop.\n")

	with open(DEV_PATH, "rb", buffering=0) as f:
		poller = select.poll()
		poller.register(f, select.POLLIN | select.POLLPRI)
		try:
			while True:
				events = poller.poll(1000)
				if not events:
					continue
				for fd, evt in events:
					alert_flag = 1 if (evt & select.POLLPRI) else 0
					if evt & (select.POLLIN | select.POLLPRI):
						data = f.read(SAMPLE_SIZE)
						if not data:
							continue
						line = decode_sample(data, alert_flag)
						samples.append(line)
						count += 1
						# Refresh screen
						os.system("clear")
						print("simtemp live view — last samples")
						print(f"(showing last {max_samples}, total samples {count})\n")
						for s in list(samples):
							print(s)
						print("\nPress Ctrl+C to stop.")
		except KeyboardInterrupt:
			print("\nStreaming stopped by user.\n")

# ---------------------------------------------------------------------------
# Input validation and configuration
# ---------------------------------------------------------------------------
def set_sampling_ms() -> None:
	"""@brief Prompt and validate sampling_ms (1–10000 ms)."""
	val = input(f"Enter value in milliseconds ({MIN_SAMPLING_MS}-{MAX_SAMPLING_MS} ms): ").strip()
	if not val.isdigit():
		print(f"{C.YELLOW}[!] Invalid input: must be an integer.{C.RESET}")
		return
	n = int(val)
	if n < MIN_SAMPLING_MS or n > MAX_SAMPLING_MS:
		print(f"{C.YELLOW}[!] Out of range. Must be between {MIN_SAMPLING_MS} and {MAX_SAMPLING_MS}.{C.RESET}")
		return
	try:
		write_attr("sampling_ms", n)
		print(f"{C.GREEN}[OK]{C.RESET} sampling_ms set to {n} ms.")
	except Exception as e:
		print(f"{C.YELLOW}[!] Error writing sampling_ms: {e}{C.RESET}")

def set_threshold_mc() -> None:
	"""@brief Prompt and validate threshold_mc (-5000–100000 mC)."""
	val = input(f"Enter value in milli ºC ({MIN_THRESHOLD_MC} to {MAX_THRESHOLD_MC} mC): ").strip()
	try:
		n = int(val)
	except ValueError:
		print(f"{C.YELLOW}[!] Invalid input: must be an integer.{C.RESET}")
		return
	if n < MIN_THRESHOLD_MC or n > MAX_THRESHOLD_MC:
		print(f"{C.YELLOW}[!] Out of range. Must be between {MIN_THRESHOLD_MC} and {MAX_THRESHOLD_MC}.{C.RESET}")
		return
	try:
		write_attr("threshold_mc", n)
		print(f"{C.GREEN}[OK]{C.RESET} threshold_mc set to {n} mC.")
	except Exception as e:
		print(f"{C.YELLOW}[!] Error writing threshold_mc: {e}{C.RESET}")

def set_mode() -> None:
	"""@brief Prompt and validate mode (normal, noisy, ramp)."""
	val = input("Enter mode behaviour [normal/noisy/ramp]: ").strip().lower()
	if val not in VALID_MODES:
		print(f"{C.YELLOW}[!] Invalid mode. Allowed: {', '.join(VALID_MODES)}{C.RESET}")
		return
	try:
		write_attr("mode", val)
		print(f"{C.GREEN}[OK]{C.RESET} mode set to '{val}'.")
	except Exception as e:
		print(f"{C.YELLOW}[!] Error writing mode: {e}{C.RESET}")

def show_stats() -> None:
	"""@brief Print contents of 'stats' attribute."""
	try:
		val = read_attr("stats")
		print(f"{C.CYAN}{val}{C.RESET}")
	except Exception as e:
		print(f"{C.YELLOW}[!] Error reading stats: {e}{C.RESET}")

# ---------------------------------------------------------------------------
# CLI Menu
# ---------------------------------------------------------------------------
def show_menu() -> None:
	"""@brief Display the available commands for the interactive CLI."""
	print(f"""
-------- Virtual Temperature Sensor driver CLI ----------
---------------------------------------------------------
       Press the corresponding key to select an option:
---------------------------------------------------------
[v] Stream temperature
[p] Set sensor sampling period (sampling_ms)
[t] Set temperature threshold (threshold_mc)
[m] Set sensor mode (normal/noisy/ramp)
[a] Show all driver attributes
[s] Show stats
[q] Exit
-------------------------
""")

def cli_loop():
	"""@brief Main interactive loop for user input and driver interaction."""
	while True:
		show_menu()
		choice = input("Select option: ").strip().lower()
		if choice == "v":
			live_stream(MAX_DISPLAY_SAMPLES)
		elif choice == "p":
			set_sampling_ms()
		elif choice == "t":
			set_threshold_mc()
		elif choice == "m":
			set_mode()
		elif choice == "a":
			list_all_attrs()
		elif choice == "s":
			show_stats()
		elif choice == "q":
			print("Exiting CLI...")
			break
		else:
			print(f"{C.YELLOW}[!] Invalid option.{C.RESET}")

# ---------------------------------------------------------------------------
# Entry Point
# ---------------------------------------------------------------------------
def main():
    """@brief Entry point for the CLI program."""
    if not SYSFS_BASE.exists():
        print(f"{C.RED}[!] Sysfs path not found: {SYSFS_BASE}{C.RESET}")
        sys.exit(1)
    if not DEV_PATH.exists():
        print(f"{C.RED}[!] Device not found: {DEV_PATH}{C.RESET}")
        sys.exit(1)

    print(f"{C.BOLD}nxp_simtemp CLI interface (v1.4){C.RESET}\n")

    # --- New argument handler ---
    if len(sys.argv) > 1:
        arg = sys.argv[1].lower()
        if arg == "v":
            live_stream(MAX_DISPLAY_SAMPLES)
            sys.exit(0)

    cli_loop()

if __name__ == "__main__":
	main()
