KERNELDIR = /lib/modules/`uname -r`/build

SYNTHS = ACNTPC ACNTSA APOLLO AUDPTR BNS DECTLK DECEXT DECPC DTLK KEYPC LTLK SOFT SPKOUT TXPRT

all: build

build:
	make -C $(KERNELDIR) M=`pwd` CONFIG_SPEAKUP=m $(SYNTHS:%=CONFIG_SPEAKUP_SYNTH_%=m)

clean:
	make -C $(KERNELDIR) M=`pwd` $@
	rm -f Module.symvers

modules_install:
	make -C $(KERNELDIR) M=`pwd` $@

