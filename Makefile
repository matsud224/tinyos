NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g
OBJCOPY		= i686-elf-objcopy
#QEMU			= qemu-system-i386
QEMU			= ~/qemu-2.10.0/i386-softmmu/qemu-system-i386 
QEMUFLAGS			= -m 512 -hda fat32disk -hdb sample -boot a

RM						= rm -f

BINDIR				= bin
OBJDIR				= obj
BOOTDIR				= boot
KERNLDSCR			= $(KERNDIR)/ldscript.ld
KERNDIR				= kernel
KERNSRC				= $(wildcard $(KERNDIR)/*.c)
KERNASM				= $(wildcard $(KERNDIR)/*.asm)
KERNOBJ				= $(addprefix $(OBJDIR)/, $(notdir $(KERNSRC:.c=.o))) $(addprefix $(OBJDIR)/, $(notdir $(KERNASM:.asm=.o)))


$(BINDIR)/kernel: $(BINDIR)/boot.bin $(BINDIR)/kernel.bin
	-mkdir -p $(BINDIR)
	cat $^ > $@

$(BINDIR)/boot.bin: $(BOOTDIR)/boot.asm
	-mkdir -p $(BINDIR)
	$(NASM) -o $@ $^

$(OBJDIR)/%.o: $(KERNDIR)/%.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -ffreestanding -c $^ -o $@

$(OBJDIR)/%.o: $(KERNDIR)/%.asm
	-mkdir -p $(OBJDIR)
	$(NASM) -f elf -o $@ $^

$(BINDIR)/kernel.elf: $(KERNOBJ) $(KERNLDSCR)
	-mkdir -p $(BINDIR)
	$(CC) -T $(KERNLDSCR) -o $@ -ffreestanding -nostdlib $(KERNOBJ) -lgcc

$(BINDIR)/kernel.bin: $(BINDIR)/kernel.elf
	-mkdir -p $(BINDIR)
	$(OBJCOPY) -O binary $^ $@


.PHONY: clean
clean:
	$(RM) $(OBJDIR)/*.o $(BINDIR)/*.elf $(BINDIR)/*.bin $(BINDIR)/kernel

.PHONY: run
run: $(BINDIR)/kernel
	$(QEMU) -fda $^ -monitor stdio -s $(QEMUFLAGS) 

