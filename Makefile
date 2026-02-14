# bsdsocktest — Amiga bsdsocket.library conformance test suite
# Cross-compilation for m68k-amigaos using bebbo's GCC

PREFIX  ?= /opt/amiga
CC       = $(PREFIX)/bin/m68k-amigaos-gcc
CFLAGS   = -noixemul -O2 -Wall -Wextra -m68020 -fomit-frame-pointer -MMD -MP
LDFLAGS  = -noixemul

OBJDIR   = obj
TARGET   = bsdsocktest

SRCS = \
	src/main.c \
	src/tap.c \
	src/testutil.c \
	src/helper_proto.c \
	src/test_socket.c \
	src/test_sendrecv.c \
	src/test_sockopt.c \
	src/test_waitselect.c \
	src/test_signals.c \
	src/test_dns.c \
	src/test_utility.c \
	src/test_transfer.c \
	src/test_errno.c \
	src/test_misc.c \
	src/test_icmp.c \
	src/test_throughput.c

OBJS = $(SRCS:src/%.c=$(OBJDIR)/%.o)

.PHONY: all clean dist

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

dist: $(TARGET)
	sh dist/build_lha.sh

-include $(wildcard $(OBJDIR)/*.d)
