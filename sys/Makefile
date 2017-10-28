NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

BINDIR		= ../bin
OBJDIR		= obj
SUBDIRS		= kern dev fs

LDSCR			= ldscript.ld

.PHONY: all
all:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) -C $$subdir || exit 1;\
	done
	-mkdir -p $(BINDIR)
	$(CC) -T $(LDSCR) -o $(BINDIR)/kernel.elf -ffreestanding -nostdlib -lgcc $(OBJDIR)/*.o
	$(OBJCOPY) -O binary $(BINDIR)/kernel.elf $(BINDIR)/kernel.bin

.PHONY: clean
clean:
	$(RM) $(BINDIR)/kernel.elf
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) clean -C $$subdir;\
	done

