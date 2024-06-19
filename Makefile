# C compiler and flags.  Adjust to taste.
CC := gcc
CFLAGS := -O3
CPPFLAGS := -Iinc
LDFLAGS :=

# Binary optimiser
STRIP := strip
SFLAGS := --strip-unneeded


EXAMPLE_BIN  := bin/evm-example
EXAMPLE_OBJS := obj/evm.o obj/example.o
EXAMPLE_LIBS :=

ASM_BIN  := bin/evm-asm
ASM_OBJS := obj/evm.o obj/asm.o
ASM_LIBS :=


# final targets
BINARIES := $(EXAMPLE_BIN) \
            $(ASM_BIN)


.PHONY: all clean


all: $(BINARIES)


obj/%.o: src/%.c
	$(COMPILE.c) -o $@ $<


$(EXAMPLE_BIN): $(EXAMPLE_OBJS)
	$(LINK.c) -o $@ $^ $(EXAMPLE_LIBS)


$(ASM_BIN): $(ASM_OBJS)
	$(LINK.c) -o $@ $^ $(ASM_LIBS)

