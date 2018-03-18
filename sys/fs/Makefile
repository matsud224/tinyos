NASM			= nasm
CC				= i686-elf-gcc
CFLAGS		= -std=gnu99 -O2 -Wall -Wextra -g
OBJCOPY		= i686-elf-objcopy
RM				= rm -f

BINDIR		= ../bin
OBJDIR		= ../obj
SUBDIRS		= fat32 minix3

.PHONY: all
all:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) -C $$subdir || exit 1;\
	done

.PHONY: clean
clean:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	$(MAKE) clean -C $$subdir;\
	done

