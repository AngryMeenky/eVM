#ifndef EVM_EVM_ASSMBLER
#  define EVM_EVM_ASSMBLER


#include <stdio.h>
#include <stdint.h>


typedef struct evm_asm_s {

} evm_assembler_t;


int evmasmParseFile(evm_assembler_t *, FILE *);
int evmasmParseLine(evm_assembler_t *, const char *);

uint32_t evmasmProgramSize(const evm_assembler_t *);

uint32_t evmasmProgramToBuffer(const evm_assembler_t *, uint8_t *, uint32_t);
int      evmasmProgramToFile(const evm_assembler_t *, FILE *);


#endif /* EVM_EVM_ASSEMBLER */

