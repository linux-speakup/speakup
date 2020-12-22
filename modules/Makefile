KERNELDIR = /lib/modules/`uname -r`/build

all: modules

modules:
	make -C $(KERNELDIR) \
	M=`pwd`/../drivers/accessibility/speakup `cat allmodule.mk` $@

modules_install:
	make INSTALL_MOD_DIR=speakup modinst_dir=speakup -C $(KERNELDIR) \
	M=`pwd`/../drivers/accessibility/speakup `cat allmodule.mk` $@

clean:
	make -C $(KERNELDIR) \
	M=`pwd`/../drivers/accessibility/speakup `cat allmodule.mk` $@
