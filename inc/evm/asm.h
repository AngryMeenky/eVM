#ifndef EVM_EVM_ASSMBLER
#  define EVM_EVM_ASSMBLER


#include "evm/config.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct evm_instruction_s {
  struct evm_instruction_s *prev;
  struct evm_instruction_s *next;
  const char               *file;
  uint32_t                  line;
  uint8_t                   binary[6];
  int8_t                    count;
  int8_t                    flags;
  char                      text[];
} evm_instruction_t;


struct evm_program_s;
typedef struct evm_program_s evm_program_t;


typedef struct evm_asm_s {
  evm_program_t     *output;
  uint32_t           flags;
  uint32_t           length;
  uint32_t           count;
  uint32_t           sequence;
  evm_instruction_t  head;
} evm_assembler_t;


EVM_API evm_assembler_t *evmasmAllocate();
EVM_API evm_assembler_t *evmasmInitialize(evm_assembler_t *);
EVM_API evm_assembler_t *evmasmFinalize(evm_assembler_t *);
EVM_API void             evmasmFree(evm_assembler_t *);

EVM_API int evmasmParseFile(evm_assembler_t *, const char *, FILE *);
EVM_API int evmasmParseLine(evm_assembler_t *, const char *, const char *, int num);

EVM_API int evmasmValidateProgram(evm_assembler_t *);

EVM_API uint32_t evmasmProgramSize(const evm_assembler_t *);

EVM_API uint32_t evmasmProgramToBuffer(const evm_assembler_t *, uint8_t *, uint32_t);
EVM_API int      evmasmProgramToFile(const evm_assembler_t *, FILE *);


#ifdef __cplusplus
}
#endif


#endif /* EVM_EVM_ASSEMBLER */

