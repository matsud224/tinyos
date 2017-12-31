BINNAME		= hello
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g -I../../
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

OBJDIR		= obj
BINDIR		= bin
SRCS			= $(wildcard *.c)
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o))) $(addprefix $(OBJDIR)/, $(notdir $(ASMS:.asm=.o)))

$(BINNAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ -ffreestanding -nostdlib -lgcc $^

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -ffreestanding -c $^ -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS) $(BINNAME)

