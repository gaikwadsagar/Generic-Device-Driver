obj-m += fcam.o

KDIR = /usr/src/kernels/2.6.35.6-45.fc14.i686

loadlib:
  insmod /root/Desktop/project/lib/videobuf-core.ko
	insmod /root/Desktop/project/lib/videobuf-vmalloc.ko

unloadlib:
	rmmod videobuf-vmalloc.ko
	rmmod videobuf-core.ko

load:
	insmod fcam.ko

unload:
	rmmod fcam.ko

sample:
	g++ sample.c -o sample `pkg-config --cflags --libs opencv`

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
	
clean:
	rm -rf *.o *ko *mod.* *.symvers *.order
