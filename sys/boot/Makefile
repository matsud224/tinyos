NASM			= nasm
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

BINDIR		= ../obj

.PHONY: all
all: $(BINDIR)/boot.o

$(BINDIR)/boot.o: boot.asm
	-mkdir -p $(BINDIR)
	$(NASM) -f elf -o $@ boot.asm

.PHONY: clean
clean:
	$(RM) $(BINDIR)/boot.o

