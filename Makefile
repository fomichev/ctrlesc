all:
	gcc -I/usr/include/libevdev-1.0/ ctrlesc.c -levdev -o ctrlesc
