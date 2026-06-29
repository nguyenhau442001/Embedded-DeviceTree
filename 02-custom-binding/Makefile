# Makefile for mydev-led kernel module
#
# Build this ON THE RASPBERRY PI, not on macOS.
# macOS lacks Linux host headers (elf.h etc.) needed by the kernel
# script build step — out-of-tree modules must be built natively.
#
# On the Pi:
#   1. sudo apt install linux-headers-$(uname -r)
#   2. Copy this directory to the Pi
#   3. make
#   4. sudo insmod mydev-led-driver.ko
#
# The DTB overlay is still cross-compiled on macOS (see 04-deploy-and-test.md).

obj-m := mydev-led-driver.o

# Kernel headers installed by linux-headers-$(uname -r) on the Pi
KDIR ?= /lib/modules/$(shell uname -r)/build

JOBS := $(shell nproc)

all:
	make -C $(KDIR) M=$(PWD) -j$(JOBS) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
