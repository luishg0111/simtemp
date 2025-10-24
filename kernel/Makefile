# SPDX-License-Identifier: GPL-2.0
# Top-level Makefile for nxp_simtemp kernel module

# Current working directory
PWD := $(shell pwd)
# Default build output
BUILD_DIR ?= $(PWD)/build
DT_SRC := dts/nxp-simtemp.dtsi
DT_OUT := $(BUILD_DIR)/nxp-simtemp.dtbo

# Detect cross-compilation prefix
# (can be set externally by build.sh)
CROSS_COMPILE ?=
ARCH ?= $(shell uname -m)
# Default kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build


# === Rules ===

all: modules dtbo postbuild

modules :
	@echo "[+] Building kernel module..."
	@mkdir -p $(BUILD_DIR)
	@$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	@$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf $(BUILD_DIR)

dtbo:
	@echo "[+] Building Device Tree Overlay..."
	@mkdir -p $(BUILD_DIR)
	dtc -@ -I dts -O dtb -o $(DT_OUT) $(DT_SRC)

postbuild:
	@echo "[+] Collecting build artifacts into $(BUILD_DIR)"
	@find . -type f \( \
		-name "*.ko" -o \
		-name "*.o" -o \
		-name '.*.cmd' -o \
            	-name '*.o.cmd' -o \
		-name "*.mod" -o \
		-name "*.mod.c" -o \
		-name '*.d' -o \
		-name "Module.symvers" -o \
		-name "modules.order" \
	\) -exec mv -f {} $(BUILD_DIR)/ \; 2>/dev/null || true
	@echo "-> Artifacts moved to $(BUILD_DIR)"

clean:
	@echo "[+] Cleaning build artifacts..."
	@$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -rf $(BUILD_DIR)
