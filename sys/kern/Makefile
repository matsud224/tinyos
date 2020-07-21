NASM			= nasm
CC				= i686-elf-gcc
MRB_CFLAGS=  -I../mruby/include -D_TINYOS_KERNEL -DMRB_WITHOUT_FLOAT -DMRB_DISABLE_STDIO -DMRB_CONSTRAINED_BASELINE_PROFILE
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g -I../ $(MRB_CFLAGS)
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

OBJDIR		= ../obj
SRCS			= $(wildcard *.c)
ASMS			= $(wildcard *.asm)
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o))) $(addprefix $(OBJDIR)/, $(notdir $(ASMS:.asm=.o)))

.PHONY: all
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

