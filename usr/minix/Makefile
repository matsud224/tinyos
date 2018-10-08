CC				= i686-tinyos-gcc
CFLAGS		= -std=gnu99 -g -O2 -Wall -Wextra -g -I../../
OBJCOPY		= i686-tinyos-objcopy
NASM			= nasm
RM				= rm -f

OBJDIR		= obj
BINDIR		= bin
BINS			= $(BINDIR)/init $(BINDIR)/sh $(BINDIR)/socktest $(BINDIR)/forktest $(BINDIR)/argvtest $(BINDIR)/ptstest $(BINDIR)/ptstest2 $(BINDIR)/tcpd $(BINDIR)/filetest


MYLIBS 		= $(OBJDIR)/socket.o $(OBJDIR)/syscall.oo $(OBJDIR)/tinyos.o


all: $(BINS)

$(BINDIR)/init: $(MYLIBS) $(OBJDIR)/init.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/sh: $(MYLIBS) $(OBJDIR)/sh.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/socktest: $(MYLIBS) $(OBJDIR)/socktest.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/forktest: $(MYLIBS) $(OBJDIR)/forktest.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/argvtest: $(MYLIBS) $(OBJDIR)/argvtest.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/ptstest: $(MYLIBS) $(OBJDIR)/ptstest.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/ptstest2: $(MYLIBS) $(OBJDIR)/ptstest2.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/tcpd: $(MYLIBS) $(OBJDIR)/tcpd.o
	$(CC) $(CFLAGS) -o $@ $^

$(BINDIR)/filetest: $(MYLIBS) $(OBJDIR)/filetest.o
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJDIR)/%.oo: %.asm
	-mkdir -p $(OBJDIR)
	$(NASM) -f elf -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJDIR)/*
	$(RM) $(BINS)

$(OBJDIR)/%.o: %.c
	-mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $^ -o $@

$(OBJDIR)/%.oo: %.asm
	-mkdir -p $(OBJDIR)
	$(NASM) -f elf -o $@ $^

.PHONY: clean
clean:
	$(RM) $(OBJDIR)/*
	$(RM) $(BINS)

