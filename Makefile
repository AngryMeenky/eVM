# C compiler and flags.  Adjust to taste.
CC := gcc
CFLAGS := -O3 -MMD
CPPFLAGS := -Iinc
LDFLAGS :=

# Binary optimiser
STRIP := strip
SFLAGS := --strip-unneeded


EXAMPLE_BIN  := bin/evm-example
EXAMPLE_OBJS := obj/evm.o obj/example.o
EXAMPLE_LIBS :=

ASM_BIN  := bin/evm-asm
ASM_OBJS := obj/evm_asm.o obj/asm.o
ASM_LIBS :=

DISASM_BIN  := bin/evm-disasm
DISASM_OBJS := obj/evm_disasm.o obj/opcodes.o obj/disasm.o
DISASM_LIBS :=


OBJECTS := $(sort $(ASM_OBJS) $(DISASM_OBJS) $(EXAMPLE_OBJS))
DEPS := $(OBJECTS:.o=.d)

# final targets
BINARIES := $(EXAMPLE_BIN) \
						$(DISASM_BIN) \
            $(ASM_BIN)


.PHONY: all clean


all: $(BINARIES)


clean:
	rm -f $(BINARIES)
	rm -f $(OBJECTS)
	rm -f $(DEPS)


obj/%.o: src/%.c
	$(COMPILE.c) -o $@ $<


$(EXAMPLE_BIN): $(EXAMPLE_OBJS)
	$(LINK.c) -o $@ $^ $(EXAMPLE_LIBS)


$(ASM_BIN): $(ASM_OBJS)
	$(LINK.c) -o $@ $^ $(ASM_LIBS)


$(DISASM_BIN): $(DISASM_OBJS)
	$(LINK.c) -o $@ $^ $(DISASM_LIBS)

-include obj/*.d

