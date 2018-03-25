SUBDIRS		= $(shell find * -maxdepth 0 -type d)
DISKIMG		= ../disk/fat32disk
DISKTYPE	= vfat
MNTDIR		= /mnt/tinyosdisk
DISKIMG2	= ../disk/minixdisk
DISKTYPE2	= minix
MNTDIR2		= /mnt/tinyosdisk2

.PHONY: all
all:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) -C $$subdir || exit 1;\
	done

.PHONY: clean
clean:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) clean -C $$subdir;\
	done

.PHONY: install
install: all
	sudo mkdir -p $(MNTDIR)
	sudo mount -t $(DISKTYPE) -o loop $(DISKIMG) $(MNTDIR)
	sudo rm -rf $(MNTDIR)/*
	sudo cp -r ./fat32/* $(MNTDIR)
	sudo umount $(MNTDIR)
	sudo mkdir -p $(MNTDIR2)
	sudo mount -t $(DISKTYPE2) -o loop $(DISKIMG2) $(MNTDIR2)
	sudo rm -rf $(MNTDIR2)/*
	sudo cp -r ./minix/* $(MNTDIR2)
	sudo umount $(MNTDIR2)

.PHONY: mount
mount:
	sudo mkdir -p $(MNTDIR)
	sudo mount -t $(DISKTYPE) -o loop $(DISKIMG) $(MNTDIR)
	sudo mkdir -p $(MNTDIR2)
	sudo mount -t $(DISKTYPE2) -o loop $(DISKIMG2) $(MNTDIR2)

.PHONY: umount
umount:
	-sudo umount $(MNTDIR)
	-sudo umount $(MNTDIR2)

.PHONY: init
init:
	sudo losetup /dev/loop0 $(DISKIMG2)
	sudo mkfs.minix -3 /dev/loop0
	sudo losetup -d /dev/loop0

