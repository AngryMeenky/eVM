#include "evm/disasm.h"
#include "evm/opcodes.h"

#include <stdlib.h>


evm_disassembler_t *evmdisAllocate() {
  return (evm_disassembler_t *) calloc(1, sizeof(evm_disassembler_t));
}


evm_disassembler_t *evmdisInitialize(evm_disassembler_t *evm) {
  if(evm) {

  }

  return evm;
}


evm_disassembler_t *evmdisFinalize(evm_disassembler_t *evm) {
  if(evm) {

  }

  return evm;
}


void evmdisFree(evm_disassembler_t *evm) {
  free(evm);
}


uint32_t evmdisFromBuffer(const evm_disassembler_t *evm, uint8_t *buffer, uint32_t length) {
  uint32_t consumed = 0;

  if(evm && buffer) {
    int ok = -1;

    while(ok && consumed < length) {
      switch(buffer[consumed]) {
        // just the opcode
        case OP_NOP:
        case OP_YIELD:
        case OP_HALT:
        case OP_PUSH_I0:
        case OP_PUSH_I1:
        case OP_PUSH_IN1:
        case OP_SWAP:
        case OP_POP_1:
        case OP_POP_2:
        case OP_POP_3:
        case OP_POP_4:
        case OP_POP_5:
        case OP_POP_6:
        case OP_POP_7:
        case OP_POP_8:
        case OP_REM_1:
        case OP_REM_2:
        case OP_REM_3:
        case OP_REM_4:
        case OP_REM_5:
        case OP_REM_6:
        case OP_REM_7:
        case OP_DUP_0:
        case OP_DUP_1:
        case OP_DUP_2:
        case OP_DUP_3:
        case OP_DUP_4:
        case OP_DUP_5:
        case OP_DUP_6:
        case OP_DUP_7:
        case OP_DUP_8:
        case OP_DUP_9:
        case OP_DUP_10:
        case OP_DUP_11:
        case OP_DUP_12:
        case OP_DUP_13:
        case OP_DUP_14:
        case OP_DUP_15:
        case OP_INC_I:
        case OP_DEC_I:
        case OP_ABS_I:
        case OP_NEG_I:
        case OP_ADD_I:
        case OP_SUB_I:
        case OP_MUL_I:
        case OP_DIV_I:
        case OP_LSH:
        case OP_RSH:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_INV:
        case OP_BOOL:
        case OP_NOT:
        case OP_CMP_I0:
        case OP_CMP_I1:
        case OP_CMP_IN1:
        case OP_CMP_I:
        case OP_RET:
        case OP_RET_1:
        case OP_RET_2:
        case OP_RET_3:
        case OP_RET_4:
        case OP_RET_5:
        case OP_RET_6:
        case OP_RET_7:
        case OP_RET_8:
        case OP_RET_9:
        case OP_RET_10:
        case OP_RET_11:
        case OP_RET_12:
        case OP_RET_13:
        case OP_RET_14:
#if EVM_FLOAT_SUPPORT == 1
        case OP_PUSH_F0:
        case OP_PUSH_F1:
        case OP_PUSH_FN1:
        case OP_INC_F:
        case OP_DEC_F:
        case OP_ABS_F:
        case OP_NEG_F:
        case OP_ADD_F:
        case OP_SUB_F:
        case OP_MUL_F:
        case OP_DIV_F:
        case OP_CONV_FI:
        case OP_CONV_FI_1:
        case OP_CONV_IF:
        case OP_CONV_IF_1:
        case OP_CMP_F0:
        case OP_CMP_F1:
        case OP_CMP_FN1:
        case OP_CMP_F:
#endif

        break;

        // opcode + 1
        case OP_BCALL:
        case OP_PUSH_8I:
        case OP_REM_R:
        case OP_JMP:
        case OP_JLT:
        case OP_JLE:
        case OP_JNE:
        case OP_JEQ:
        case OP_JGE:
        case OP_JGT:
        case OP_RET_I:
        case OP_JTBL:
        case OP_LJTBL:

        break;

        // opcode + 2
        case OP_CALL:
        case OP_PUSH_16I:
        case OP_LJMP:
        case OP_LJLT:
        case OP_LJLE:
        case OP_LJNE:
        case OP_LJEQ:
        case OP_LJGE:
        case OP_LJGT:

        break;

        // opcode + 3
        case OP_LCALL:
        case OP_PUSH_24I:

        break;

        // opcode + 4
        case OP_PUSH_32I:
#if EVM_FLOAT_SUPPORT == 1
        case OP_PUSH_F:
#endif

        break;

        default:
          ok = 0;
        break;
      }
    }
  }

  return consumed;
}


int evmdisFromFile(const evm_disassembler_t *evm, FILE *fp) {
  uint8_t *buffer;
  int result = -1;
  long size;

  if(evm && fp) {
    if(!fseek(fp, 0L, SEEK_END) && (size = ftell(fp)) >= 0 && !fseek(fp, 0L, SEEK_SET)) {
      if((buffer = malloc(size))) {
        if(fread(buffer, 1, size, fp) == (size_t) size) {
          result = evmdisFromBuffer(evm, buffer, (uint32_t) size) == (uint32_t) size ? 0 : -1;
        }

        free(buffer);
      }
    }
  }

  return result;
}

