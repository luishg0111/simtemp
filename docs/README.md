# Simtemp project

This project implements a **virtual temperature sensor** in the Linux Kernel, simulating a platform driver that generates periodic temperature sambples and exposes them to user space, with a user app to configure/read it.

## Goals 

- **Kernel module (C)**: out\-of\-tree platform driver producing periodic "temperature" samples.
- **User-Kernel communication**: character device with 'read()' + 'poll/epoll'; configuration via 'sysfs' and/or 'ioctl'.
- **Device Tree (DT)**: DTS snippet + proper bindign and property parsing at kernel driver.
- **User space app (Python **or** C++**): CLI required; optional GUI to visualize reading/alerts.
- **Shell scripts**: build, run demo, (optional) lint.
- **Design quality**: modularity, locking choices, API contract, problem\-solving write\-ups.

## Build steps
Run script:
```bash
    ./scripts/build.sh
```
> Note: verify execution permisions: ```bash chmod +x scripts/build.sh```

## Run steps
