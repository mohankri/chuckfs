ifeq ($(KSRC),)
	KSRC := /lib/modules/$(shell uname -r)/build
endif

ifeq ($(KVER),)
        KVER := $(shell uname -r)
endif

obj-m += arxcis_b.o

all:
	$(MAKE) -C$(KSRC) M=$(PWD)

install:
	install -o root -g root -m 0755 arxcis_b.ko $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/
	depmod -a

uninstall:
	rm -f $(DESTDIR)/lib/modules/$(KVER)/kernel/drivers/block/arxcis_b.ko
	depmod -a

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions .arxcis_b.o.d
