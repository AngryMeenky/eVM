#include "evm/evm_asm.h"
#include "evm/opcodes.h"


int evmasmParseFile(evm_assembler_t *evm, FILE *fp) {
  return -1;
}


int evmasmParseLine(evm_assembler_t *evm, const char *line) {
  return -1;
}


uint32_t evmasmProgramSize(const evm_assembler_t *evm) {
  return 0;
}


uint32_t evmasmProgramToBuffer(const evm_assembler_t *evm, uint8_t *buf, uint32_t max) {
  return 0;
}


int evmasmProgramToFile(const evm_assembler_t *evm, FILE *fp) {
  return -1;
}

