# C compiler and flags.  Adjust to taste.
CC := gcc
CFLAGS := -MMD -Wall -Wextra -Werror -fno-strict-aliasing
CPPFLAGS := -Iinc
LDFLAGS :=

SCONS_JOBS := 8


ifneq ($(filter debug,$(MAKECMDGOALS)),)
  # debug specific flags/options
  CFLAGS += -g
  DO_STRIP := 0
else
  # release specific flags/options
  CFLAGS += -O3
  DO_STRIP := 1
endif

# Binary optimiser
STRIP := strip
SFLAGS := --strip-unneeded


EXAMPLE_BIN  := bin/evm-example
EXAMPLE_OBJS := obj/evm.o obj/example.o obj/evm_disasm.o
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


.PHONY: all clean debug release gdextension-linux gdextension-macos gdextension-windows


release: all
debug: all


all: $(BINARIES)


clean:
	rm -f $(BINARIES)
	rm -f $(OBJECTS)
	rm -f $(DEPS)


obj/%.o: src/%.c
	$(COMPILE.c) -o $@ $<


$(EXAMPLE_BIN): $(EXAMPLE_OBJS)
	$(LINK.c) -o $@ $^ $(EXAMPLE_LIBS)
ifeq ($(DO_STRIP),1)
	$(STRIP) $(SFLAGS) $@
endif

$(ASM_BIN): $(ASM_OBJS)
	$(LINK.c) -o $@ $^ $(ASM_LIBS)
ifeq ($(DO_STRIP),1)
	$(STRIP) $(SFLAGS) $@
endif


$(DISASM_BIN): $(DISASM_OBJS)
	$(LINK.c) -o $@ $^ $(DISASM_LIBS)
ifeq ($(DO_STRIP),1)
	$(STRIP) $(SFLAGS) $@
endif


-include obj/*.d


gdextension-linux-debug: godot-cpp/bin/libgodot-cpp.linux.template_debug.x86_64.a
gdextension-linux-release: godot-cpp/bin/libgodot-cpp.linux.template_release.x86_64.a
 

godot-cpp/bin/libgodot-cpp.linux.template_debug.x86_64.a:	godot-cpp/extension_api.json
	cd godot-cpp && scons platform=linux custom_api_file=extension_api.json -j $(SCONS_JOBS)


godot-cpp/extension_api.json:
	cd godot-cpp && godot --dump-extension-api

