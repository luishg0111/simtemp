# Target module name
obj-m := nxp_simtemp.o

# Default kernel build directory (current running kernel)
KDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

# Default rule
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean rule
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Info rule
info:
	@echo "Building for kernel: $(shell uname -r)"
	@echo "Using KDIR: $(KDIR)"