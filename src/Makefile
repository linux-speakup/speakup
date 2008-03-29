ifneq ($(KERNELRELEASE),)
include Kbuild
else

KERNELDIR = /lib/modules/`uname -r`/build

all: build

build:
	make -C $(KERNELDIR) M=`pwd` `cat allmodule.mk`

clean:
	make -C $(KERNELDIR) M=`pwd` `cat allmodule.mk` $@

modules_install:
	make -C $(KERNELDIR) M=`pwd` `cat allmodule.mk` $@

endif
