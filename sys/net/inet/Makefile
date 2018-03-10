NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -fno-strict-aliasing -g -I../../
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

OBJDIR		= ../../obj
SRCS			= $(wildcard *.c)
ASMS			= $(wildcard *.asm)
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o))) $(addprefix $(OBJDIR)/, $(notdir $(ASMS:.asm=.o)))

all: $(OBJS)

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -ffreestanding -c $^ -o $@

$(OBJDIR)/%.o: %.asm
	-mkdir -p $(OBJDIR)
	$(NASM) -f elf -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS)

