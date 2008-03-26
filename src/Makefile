ifneq ($(KERNELRELEASE),)
include Kbuild
else

include allmodule.mk

KERNELDIR = /lib/modules/`uname -r`/build

all: build

build:
	make -C $(KERNELDIR) M=`pwd`

clean:
	make -C $(KERNELDIR) M=`pwd` $@
	rm -f Module.symvers

modules_install:
	make -C $(KERNELDIR) M=`pwd` $@

endif
