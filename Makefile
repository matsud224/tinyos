NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g
OBJCOPY		= i686-elf-objcopy
QEMU			= qemu-system-i386
SUDO			= sudo
QEMUFLAGS			= -m 512 -hda disk/minixdisk -hdc disk/fat32disk -serial stdio -monitor telnet:127.0.0.1:11111,server,nowait
QEMUNETFLAGS	= -net nic,model=rtl8139 -net tap,ifname=tap0,script=ifup.sh
RM						= rm -f

BINDIR				= bin
SYSDIR				= sys
USRDIR				= usr

CRTI_OBJ		 	= crti.o
CRTBEGIN_OBJ	:= $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ		:= $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ			= crtn.o

KERN_ELF      = $(BINDIR)/kernel.elf

.PHONY: all
all:
	-mkdir -p $(BINDIR)
	$(MAKE) -C $(SYSDIR)

.PHONY: clean
clean:
	$(RM) $(BINDIR)/*.elf
	$(MAKE) clean -C $(SYSDIR)

.PHONY: run
run: all
	$(QEMU) -kernel $(KERN_ELF) -s $(QEMUFLAGS)

.PHONY: run-with-network
run-with-network: all
	$(SUDO) $(QEMU) -kernel $(KERN_ELF) -s $(QEMUFLAGS) $(QEMUNETFLAGS)
