# Simtemp project

This project implements a **virtual temperature sensor** in the Linux Kernel, simulating a platform driver that generates periodic temperature sambples and exposes them to user space, with a user app to configure/read it.

## Goals 

- **Kernel module (C)**: out\-of\-tree platform driver producing periodic "temperature" samples.
- **User-Kernel communication**: character device with 'read()' + 'poll/epoll'; configuration via 'sysfs' and/or 'ioctl'.
- **Device Tree (DT)**: DTS snippet + proper bindign and property parsing at kernel driver.
- **User space app (Python **or** C++**): CLI required; optional GUI to visualize reading/alerts.
- **Shell scripts**: build, run demo, (optional) lint.
- **Design quality**: modularity, locking choices, API contract, problem\-solving write\-ups.

## Repo link
[Simtemp](https://github.com/luishg0111/simtemp.git)

## Runnig demo video
[Demo video](https://drive.google.com/drive/folders/1BsgzZf4kmibJIMd4puhhpD84Ya_uJwOU?usp=sharing)

---
## Prerequisites

Before building or running the project, install the required packages:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    linux-headers-$(uname -r) \
    gcc make \
    clang \
    python3 python3-pip \
    git \
    bc \
    cpio flex bison \
    crossbuild-essential-arm64 \
    qemu-system-arm qemu-efi \
    shellcheck
```

These ensure you can:
- Build kernel modules for host or cross-compilation targets.
- Run QEMU virtual targets.
- Validate code style with `lint.sh`.

This demo can be run in Ubuntu **Ubuntu 24.04 LTS (x86_64 or ARM64)** for easier validation. 

To run in qemu mount a maquine with:

```
#!/bin/bash
qemu-system-aarch64 \
   -smp $(nproc) \
   -M virt -cpu cortex-a57 -m 4G \
   -nographic \
   -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
   -drive if=virtio,file=root.img,format=raw,media=disk \
   -device virtio-net-device,netdev=user0 \
   -netdev user,id=user0,hostfwd=tcp::2222-:22
```
---
## Build steps
## Build Steps

The script `scripts/build.sh` automates module compilation for different targets.
It supports three build modes via command-line parameter:

| Mode | Command | Description |
|------|----------|-------------|
| host | `./build.sh host` | Build for the current system kernel (default). |
| qemu | `./build.sh qemu` | Cross-compile for a QEMU ARM64 environment. |
| rbpi | `./build.sh rbpi` | Cross-compile for Raspberry Pi 3B+. |
| clean | `./build.sh clean` | Clean compilation artifacts. |

#### Build for host system (x86_64)

```bash  
cd scripts  
./build.sh host  
```

#### Cross-compile for QEMU ARM64

```bash  
cd scripts  
./build.sh qemu  
```

#### Cross-compile for Raspberry Pi 3B+

```bash  
cd scripts  
./build.sh rbpi  
```

#### Clean previous builds

```bash  
cd scripts  
./build.sh clean  
```

All compiled modules are generated in:  
```  
kernel/build/nxp_simtemp.ko  
```
> Note: verify execution permisions: ```bash chmod +x *.sh```

---
## Running Steps

Once the module is compiled successfully, use the following scripts to test and run the project.

### Run Demo Mode (auto reload + CLI windows)

**Script:** `scripts/run_demo.sh`  
**Privilege:** `sudo` required (for insmod/rmmod)

```bash  
cd scripts  
sudo ./run_demo.sh  
```

This script will:

1. Check if the module `nxp_simtemp` is already loaded.
    
2. If so, unload it (`rmmod`) and reload a fresh instance.
    
3. Launch **two terminal windows**:  
    - One with the interactive CLI (`simtemp_cli.py`)  
    - One in live-streaming mode (`simtemp_cli.py v`)
    
**Expected outcome:**  
- `/dev/simtemp` created.  
- `/sys/class/simtemp/simtemp0/` available.  
- CLI consoles show live temperature updates and allow configuration.

---
###  Run Automated Test Suite

**Script:** `test/run_test.sh`  
**Privilege:** `sudo` required (for module reloads).

```bash  
cd test  
sudo ./run_test.sh  
```

Runs test cases **T1–T6** automatically:  
- Load/unload integrity  
- Sampling frequency  
- Threshold + POLLPRI  
- Error handling  
- Concurrency safety  
- API contract (partial reads)

All tests print `[OK]` / `[ERR]` status and summary at the end.


---
## Code Quality Check — Linting

To ensure the code meets Linux kernel style conventions, run:

**Prerequisite:**
```
sudo apt-get update
apt-get source linux-image-$(uname -r)
```
**Script:** `scripts/lint.sh`

```bash  
cd scripts  
./lint.sh  
```

This script executes:  
- `checkpatch.pl` on kernel sources.  
- `shellcheck` for shell scripts.  
- Aggregates warnings into `lint_report.log`.

**Outputs:**  
```  
scripts/lint_report.log  
```

Review this file for any remaining coding style or documentation issues.