NASM			= nasm
CC				= i686-elf-gcc
MRB_CFLAGS=  -I../mruby/include -D_TINYOS_KERNEL -DMRB_WITHOUT_FLOAT -DMRB_DISABLE_STDIO -DMRB_CONSTRAINED_BASELINE_PROFILE
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g -I../ $(MRB_CFLAGS)
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

MRBBINDIR = ../../mruby/build/host/bin
MRBC      = $(MRBBINDIR)/mrbc

OBJDIR		= ../../obj
SRCS			= $(wildcard *.rb)
CSRCS			= $(notdir $(SRCS:.rb=.c))
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(SRCS:.rb=.o)))

.PHONY: all
all: $(OBJS)

%.c: %.rb
	$(MRBC) -Bmrb_irep_of_$(basename $^) $^

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -ffreestanding -c $^ -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS) $(CSRCS)

