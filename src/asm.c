#include "evm/opcodes.h"
#include "evm/asm.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  evm_assembler_t *assembler = evmasmInitialize(evmasmAllocate());
  int result = EXIT_FAILURE;
  int arg;

  if(assembler) {
    for(result = EXIT_SUCCESS, arg = 1; result == EXIT_SUCCESS && arg < argc; ++arg) {
      FILE *src = fopen(argv[arg], "r");

      if(!src) {
        fprintf(stderr, "%s: failed to open %s for reading\n", *argv, argv[arg]);
        result = EXIT_FAILURE;
      }
      else if(evmasmParseFile(assembler, argv[arg], src)) {
        fprintf(stderr, "%s: failed to successfully parse %s\n", *argv, argv[arg]);
        result = EXIT_FAILURE;
      }

      fclose(src);
    }

    if(result == EXIT_SUCCESS) {
      if(!evmasmValidateProgram(assembler)) {
        if(evmasmProgramToFile(assembler, stdout)) {
          fprintf(stderr, "%s: program failed to output\n", *argv);
          result = EXIT_FAILURE;
        }
      }
      else {
        fprintf(stderr, "%s: program failed to validate\n", *argv);
        result = EXIT_FAILURE;
      }
    }

    evmasmFree(evmasmFinalize(assembler));
  }

  return result;
}

