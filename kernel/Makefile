# Default kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

# Default rule
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean rule
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean