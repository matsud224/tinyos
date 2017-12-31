BINNAME		= hello
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g -I../../
OBJCOPY		= i686-elf-objcopy
NASM			= nasm
RM				= rm -f

OBJDIR		= obj
BINDIR		= bin
SRCS			= $(wildcard *.c)
ASMS			= $(wildcard *.asm)
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o))) $(addprefix $(OBJDIR)/, $(notdir $(ASMS:.asm=.o)))

$(BINNAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ -ffreestanding -nostdlib -lgcc $^

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -ffreestanding -c $^ -o $@

$(OBJDIR)/%.o: %.asm
	-mkdir -p $(OBJDIR)
	$(NASM) -f elf -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS) $(BINNAME)

