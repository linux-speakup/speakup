KERNELDIR = /lib/modules/`uname -r`/build

all: build

build:
	make -C $(KERNELDIR) M=`pwd`

clean:
	make -C $(KERNELDIR) M=`pwd` clean
	rm -f Module.symvers

