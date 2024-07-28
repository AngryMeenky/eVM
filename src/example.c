#include "evm.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


static int slurp(FILE *fp, uint8_t **buf, uint32_t *len, const char *exe, const char *name);
static int32_t programChecksum(evm_t *vm);
static int32_t programDump(evm_t *vm);
static int32_t stackDump(evm_t *vm);

int main(int argc, char **argv) {
  int result = EXIT_SUCCESS;
  int arg;

  if(argc > 1) {
    evm_t vm;

    if(evmInitialize(&vm, NULL, 1024U)) {
      for(arg = 1; arg < argc; ++arg) {
        FILE *input = fopen(argv[arg], "rb");

        if(input) {
          uint8_t  *prog;
          uint32_t  length;
          if(!slurp(input, &prog, &length, *argv, argv[arg])) {
            if(evmSetProgram(&vm, prog, length)) {
              do {
                evmRun(&vm, 32768U);
              } while(!evmHasHalted(&vm));
            }
            else {
              fprintf(stderr, "%s: Failed to set program for eVM\n", *argv);
              result = EXIT_FAILURE;
            }

            free(prog);
          }
          else {
            result = EXIT_FAILURE;
          }

          fclose(input);
        }
        else {
          fprintf(stderr, "%s: Failed to open %s for reading\n", *argv, argv[arg]);
          result = EXIT_FAILURE;
        }
      }
      evmFinalize(&vm);
    }
    else {
      fprintf(stderr, "%s: Failed to initialize eVM\n", *argv);
      result = EXIT_FAILURE;
    }
  }
  else {
    fprintf(stderr, "Usage: %s PROG...\n", *argv);
    result = EXIT_FAILURE;
  }

  return result;
}


// builtin bindings
const EvmBuiltinFunction EVM_BUILTINS[EVM_MAX_BUILTINS] = {
  &programChecksum,
  &stackDump,
  &programDump,
  &evmUnboundHandler,
  &evmUnboundHandler,
  &evmUnboundHandler,
  &evmUnboundHandler,
  &evmUnboundHandler,
};


static int slurp(FILE *fp, uint8_t **buffer, uint32_t *length, const char *exe, const char *name) {
  long size;
  *length = 0U;
  *buffer = NULL;

  if(!fseek(fp, 0L, SEEK_END) && (size = ftell(fp)) >= 0 && !fseek(fp, 0L, SEEK_SET)) {
    if((*buffer = malloc(size))) {
      if(fread(buffer, 1, size, fp) == (size_t) size) {
        *length = (uint32_t) size;
      }
      else {
        free(*buffer);
        *buffer = NULL;
        fprintf(stderr, "%s: Failed to read  program from %s\n", exe, name);
      }
    }
    else {
      fprintf(stderr, "%s: Failed to allocate buffer for program\n", exe);
    }
  }
  else {
    fprintf(stderr, "%s: Failed to obtain file size for %s\n", exe, name);
  }

  return !*length;
}


static int32_t programChecksum(evm_t *vm) {
  int32_t sum = 0;
  uint32_t ip;

  for(ip = 0U; ip < vm->maxProgram; ++ip) {
    sum = ((sum << 1) + vm->program[ip]) ^ ((sum >> 31) & 1);
  }

  return sum;
}


static int32_t programDump(evm_t *vm) {
  (void) vm;
  return 0;
}


static int32_t stackDump(evm_t *vm) {
  uint32_t sp;
  printf("Stack: %u\n", vm->sp);
  for(sp = vm->sp; sp-- > 0;) {
    printf("  %08X\n", vm->stack[sp]);
  }

  return 0;
}

