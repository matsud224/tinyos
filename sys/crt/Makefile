CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g -I../
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

OBJDIR		= ./
ASMS			= $(wildcard *.s)
OBJS			= $(addprefix $(OBJDIR)/, $(notdir $(ASMS:.s=.o)))

.PHONY: all
all: $(OBJS)

$(OBJDIR)/%.o: %.s
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS)

