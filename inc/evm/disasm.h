#ifndef EVM_EVM_DISASSMBLER
#  define EVM_EVM_DISASSMBLER


#include "evm/config.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct evm_disasm_inst_s {
  struct evm_disasm_inst_s *prev;
  struct evm_disasm_inst_s *next;
  uint32_t                 *targets;
  uint32_t                  offset;
  union {
    uint8_t                 raw[4];
    int8_t                  i8;
    int16_t                 i16;
    int32_t                 i32;
#if EVM_FLOAT_SUPPORT == 1
    float                   f32;
#endif
    uint8_t                 pair[2];
  }                         arg;
  uint8_t                   opcode;
  int8_t                    label;
} evm_disasm_inst_t;


typedef struct evm_disasm_s {
  evm_disasm_inst_t instructions;
} evm_disassembler_t;


EVM_API evm_disassembler_t *evmdisAllocate();
EVM_API evm_disassembler_t *evmdisInitialize(evm_disassembler_t *);
EVM_API evm_disassembler_t *evmdisFinalize(evm_disassembler_t *);
EVM_API void                evmdisFree(evm_disassembler_t *);

EVM_API uint32_t evmdisFromBuffer(evm_disassembler_t *, const uint8_t *, uint32_t);
EVM_API int      evmdisFromFile(evm_disassembler_t *, FILE *);

EVM_API int      evmdisToBuffer(const evm_disassembler_t *, char **, int *);
EVM_API int      evmdisToFile(const evm_disassembler_t *, FILE *);


#ifdef __cplusplus
}
#endif


#endif /* EVM_EVM_ASSEMBLER */


