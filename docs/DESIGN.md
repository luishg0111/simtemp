# System Architecture 
## Diagram System Architecture Overview

Following diagram shows te main components of the **“Virtual Sensor + Alert Path”**
```mermaid
---
config:
  theme: redux
  layout: dagre
---
flowchart TB
 subgraph s1["User Space"]
        n2["User Interface (CLI/GUI)"]
  end
 subgraph s2["Kernel Space"]
        n3["nxp_simtemp.ko"]
  end
 subgraph s3["Device tree"]
        n4["nxp-simtemp.dtsi"]
  end
    n2 -- write(), read(), poll() --> n3
    n3 -- "<span style=color:>of_property_read_*()</span>" --> n4
    n3 -- show(), store(), copy_to_user() --> n2
    n4 -- "<span style=color:>provides properties</span>" --> n3
    n4@{ shape: rect}
```

## Detailed Architecture Design
```mermaid
---
config:
  layout: elk
---
flowchart TB
 subgraph US["User Space"]
        U1["CLI / GUI App"]
        U2["Shell Scripts"]
        U3["sysfs Access<br>echo/cat /sys/class/simtemp/*"]
        U4["Character Device<br>(/dev/simtemp)<br>"]
  end
 subgraph KS["Kernel Space:<br>nxp_simtemp.ko"]
        K1["platform_driver struct<br>registered via module_platform_driver()"]
        K2["probe(struct platform_device *pdev)<br>called when DT 'compatible' matches" ]
        K3["of_property_read_u32()<br>read 'sampling-ms', 'threshold-mC'" ]
        K4["devm_kzalloc()<br>allocate device context"]
        K5["hrtimer_init() / hrtimer_start()\n periodic sampling"]
        K6["nxp_simtemp_timer_callback()<br>simulates temperature"]
        K7["ring_buffer struct<br>stores samples &amp; flags"]
        K8["wait_queue_head_t<br>used by poll() to wake user readers"]
        K9["sysfs_create_group() <br>create attributes"]
        K10["device_create()/class_create()<br>create /dev/simtemp node"]
        K11["file_operations struct<br>defines read(), poll(), unlocked_ioctl()"]
        K13["hrtimer_cancel() / cleanup\ remove() path"]
  end
 subgraph DT["Device Tree:nxp-simtemp.dtsi"]
        D1["simtemp@0"]
        D2["compatible = \nxp,simtemp\"]
        D3["sampling-ms = &lt;100&gt;"]
        D4["threshold-mC = &lt;45000&gt;"]
        D5["mode = \normal\"]
  end
    U1 --> U4 & U3
    U4 == "<span style=background-color:>read(), poll(), ioctl()</span>" ==> K10
    U3 == sysfs ==> K9
    U2 == "insmod / rmmod nxp_simtemp.ko" ==> K1
    K1 -- match compatible string --> K2
    K3 -- read properties --> K2
    K2 -- allocate context --> K4
    K2 -- initialize hrtimer --> K5
    K5 -- callback --> K6
    K6 -- write sample --> K7
    K6 -- wake readers --> K8
    K7 -- available data --> K11
    K8 -- wake_up_interruptible() --> K11
    K10 -- copy_to_user() --> U4
    K9 -- attribute store() callbacks --> D1
    K9 -- attribute show() callbacks --> U3
    K4 -- cleanup --> K13
    D1 -. provides properties .-> K2
    D2 -. enables match .-> K1
    D3 -. default sampling .-> K3
    D4 -. default threshold .-> K3
    D5 -. mode parameter .-> K3
    K10 --> K2
    D1 --> D2 & D3 & D4 & D5
    K2@{ shape: rect}
    K3@{ shape: rect}
```

## System Flow Overview
### 1.Initialization Phase
1. The kernel parses the **Device Tree (DT)** when load nxp_simtemp.ko module (`insmod`), .
2. When a DT node with `compatible = "nxp,simtemp"` is found, the kernel creates a corresponding `platform_device` instance.
3. **nxp_simtemp platform driver** registers itself using `platform_driver_register()`.
4. Kernel matches the DT node to the driver’s `of_match_table`, triggering the driver’s `probe()` callback.
5. Inside `probe()`, the driver:
   - Reads configuration parameters from the DT with `of_property_read_*()` API.
   - Allocates memory for internal structures (buffer, state).
   - Initializes the `hrtimer` or workqueue responsible for periodic temperature sample generation.
   - Creates **sysfs entries** for runtime configuration and exposes a **character device** for user-space access.

## 2. Runtime Operation
1. The `hrtimer` or workqueue triggers periodically, every *N* milliseconds, as defined by the `sampling-ms` property.
2. On each trigger:
   - A new simulated temperature value is processed.
   - The sample is stored into a **ring buffer**.
   - When threshold exceeded, a **wait queue** wakes up user-space readers waiting via `poll()` or `read()`.
3. The driver continuously updates internal metrics (e.g. timestamp) as part of the simulation logic.

## 3. User-Space Interaction
### User interface (CLI/GUI)
1. Character Device Interface (`/dev/simtemp`)
    - **User interface* use `open()`, `read()`, `poll()`, or `ioctl()` system calls.
    - `read()` copies the latest temperature samples to user space via `copy_to_user()`.
    - `poll()` allows applications to wait for new data events asynchronously.

2. Sysfs Interface (`/sys/class/simtemp/`)
    - User can modify configuration parameters dynamically through **User interface** e.g:
      ```bash
      echo 500 > /sys/class/simtemp/sampling_ms
      echo 42000 > /sys/class/simtemp/threshold_mC
      ```


