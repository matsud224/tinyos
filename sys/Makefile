NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

BINDIR		= ../bin
OBJDIR		= obj
SUBDIRS		= kern dev fs crt net

LDSCR			= ldscript.ld

MAP				= $(BINDIR)/kernel.map

CRTI_OBJ      = crt/crti.o
CRTBEGIN_OBJ  := $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ    := $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ      = crt/crtn.o

MRUBY_LIB     = mruby/build/tinyos/lib/libmruby.a

.PHONY: all
all:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) -C $$subdir || exit 1;\
	done
	-mkdir -p $(BINDIR)
	$(CC) -T $(LDSCR) -o $(BINDIR)/kernel.elf -Wl,-Map=$(MAP) -ffreestanding -nostdlib -lgcc $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJDIR)/*.o $(CRTEND_OBJ) $(CRTN_OBJ)
	$(OBJCOPY) -O binary $(BINDIR)/kernel.elf $(BINDIR)/kernel.bin

.PHONY: clean
clean:
	$(RM) $(BINDIR)/kernel.elf
	$(RM) $(OBJDIR)/*.o
	$(RM) $(MAP)
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) clean -C $$subdir;\
	done

