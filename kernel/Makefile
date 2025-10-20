# Default kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)
BUILD_DIR="${PWD}/build"

# Default rule
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	mkdir -p $(BUILD_DIR)

# Clean rule
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf $(BUILD_DIR)/

.PHONY: all clean