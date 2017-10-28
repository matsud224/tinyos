NASM			= nasm
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

BINDIR		= ../bin

.PHONY: all
all: $(BINDIR)/boot.bin

$(BINDIR)/boot.bin:
	-mkdir -p $(BINDIR)
	$(NASM) -o $@ boot.asm

.PHONY: clean
clean:
	$(RM) $(BINDIR)/boot.bin

