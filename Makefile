all: ramdisk

ramdisk: ramdisk.c 
	gcc -Wall -g ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk

clean:
	rm ramdisk
