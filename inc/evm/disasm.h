#ifndef EVM_EVM_DISASSMBLER
#  define EVM_EVM_DISASSMBLER


#include "evm/evm_config.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct evm_disasm_s {
  uint8_t *blob;
} evm_disassembler_t;


EVM_API evm_disassembler_t *evmdisAllocate();
EVM_API evm_disassembler_t *evmdisInitialize(evm_disassembler_t *);
EVM_API evm_disassembler_t *evmdisFinalize(evm_disassembler_t *);
EVM_API void             evmdisFree(evm_disassembler_t *);

EVM_API int evmdisParseFile(evm_disassembler_t *, FILE *);

EVM_API uint32_t evmdisToBuffer(const evm_assembler_t *, uint8_t *, uint32_t);
EVM_API int      evmdisToFile(const evm_assembler_t *, FILE *);


#ifdef __cplusplus
}
#endif


#endif /* EVM_EVM_ASSEMBLER */


