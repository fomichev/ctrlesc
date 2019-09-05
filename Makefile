all:
	gcc -I/usr/include/libevdev-1.0/ ctrlesc.c -levdev -o ctrlesc

install:
	sudo chown root:$$GROUPS ctrlesc
	sudo chmod u+s ctrlesc
	systemctl --user enable ctrlesc.service
	systemctl --user restart ctrlesc.service

status:
	systemctl --user status ctrlesc.service
