# Default kernel build directory
KDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all default: modules
install: modules_install

modules modules_install help clean:
	$(MAKE) -C $(KDIR) M=$(PWD) $@