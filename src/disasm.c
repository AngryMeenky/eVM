#include "evm/opcodes.h"
#include "evm/disasm.h"

#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv) {
  int result = EXIT_SUCCESS;
  int arg;

  for(arg = 1; arg < argc; ++arg) {
    FILE *src = fopen(argv[arg], "rb");

    if(src) {
      evm_disassembler_t *disasm = evmdisInitialize(evmdisAllocate());

      if(disasm) {
        if(!evmdisFromFile(disasm, src)) {
          fprintf(stdout, "; generated from %s\n", argv[arg]);
          evmdisToFile(disasm, stdout);
        }
        else {
          result = EXIT_FAILURE;
          fprintf(stderr, "%s: Failed to disassemble %s\n", *argv, argv[arg]);
        }

        evmdisFree(evmdisFinalize(disasm));
      }
      else {
        result = EXIT_FAILURE;
        fprintf(stderr, "%s: Unable to create disassembler for %s\n", *argv, argv[arg]);
      }
    }
    else {
      result = EXIT_FAILURE;
      fprintf(stderr, "%s: Unable to open %s for reading\n", *argv, argv[arg]);
    }
  }

  return result;
}

