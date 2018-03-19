NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g
OBJCOPY		= i686-elf-objcopy
QEMU			= qemu-system-i386 
SUDO			= sudo
QEMUFLAGS			= -m 512 -hda disk/fat32disk -hdb disk/sample -hdc disk/minixdisk -boot a -serial stdio -monitor telnet:127.0.0.1:11111,server,nowait
QEMUNETFLAGS	= -net nic,model=rtl8139 -net tap,ifname=tap0,script=ifup.sh
RM						= rm -f

BINDIR				= bin
BOOTDIR				= boot
SYSDIR				= sys
USRDIR				= usr

CRTI_OBJ		 	= crti.o
CRTBEGIN_OBJ	:= $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ		:= $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ			= crtn.o

.PHONY: all
all:
	-mkdir -p $(BINDIR)
	$(MAKE) -C $(BOOTDIR)
	$(MAKE) -C $(SYSDIR)
	$(MAKE) -C $(USRDIR) install
	cat $(BINDIR)/boot.bin $(BINDIR)/kernel.bin > $(BINDIR)/kernel
  
.PHONY: clean
clean:
	$(RM) $(BINDIR)/*.elf $(BINDIR)/*.bin $(BINDIR)/kernel
	$(MAKE) clean -C $(SYSDIR)

.PHONY: run
run: all
	$(QEMU) -fda $(BINDIR)/kernel -s $(QEMUFLAGS) 

.PHONY: run-with-network
run-with-network: all
	$(SUDO) $(QEMU) -fda $(BINDIR)/kernel -s $(QEMUFLAGS) $(QEMUNETFLAGS)

