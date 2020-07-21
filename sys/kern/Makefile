NASM			= nasm
CC				= i686-elf-gcc
MRB_CFLAGS=  -I../mruby/include -D_TINYOS_KERNEL -DMRB_WITHOUT_FLOAT -DMRB_DISABLE_STDIO -DMRB_CONSTRAINED_BASELINE_PROFILE
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g -I../ $(MRB_CFLAGS)
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

OBJDIR		= ../obj
MRBSRCDIR = mrbscript
SRCS			= $(wildcard *.c)
ASMS			= $(wildcard *.asm)
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.c=.o))) $(addprefix $(OBJDIR)/, $(notdir $(ASMS:.asm=.o)))

.PHONY: all
all: mrbscript_list.h $(OBJS)

mrbscript_list.h:
	$(MAKE) -C mrbscript all
	bash gen_mrbscript_list.sh

.PHONY: rerun-mrb
rerun-mrb:
	$(RM) mrbscript_list.h
	$(RM) $(OBJDIR)/kernmrb.o
	$(MAKE)

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -ffreestanding -c $^ -o $@

$(OBJDIR)/%.o: %.asm
	-mkdir -p $(OBJDIR)
	$(NASM) -f elf -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJS) mrbscript_list.h

