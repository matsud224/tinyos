SUBDIRS		= $(shell find * -maxdepth 0 -type d)
DISKIMG		= ../disk/fat32disk
DISKTYPE	= vfat
MNTDIR		= /mnt/tinyosdisk

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
	-sudo mkdir $(MNTDIR)
	sudo mount -t $(DISKTYPE) -o loop $(DISKIMG) $(MNTDIR)
	sudo cp -r ./* $(MNTDIR)
	sudo umount $(MNTDIR)

.PHONY: mount
mount:
	-sudo mkdir $(MNTDIR)
	sudo mount -t $(DISKTYPE) -o loop $(DISKIMG) $(MNTDIR)

.PHONY: unmount
unmount:
	sudo umount $(MNTDIR)

