#include "evm/disasm.h"
#include "evm/opcodes.h"

#include <stdlib.h>
#include <string.h>


evm_disassembler_t *evmdisAllocate() {
  return (evm_disassembler_t *) calloc(1, sizeof(evm_disassembler_t));
}


evm_disassembler_t *evmdisInitialize(evm_disassembler_t *evm) {
  if(evm) {
    memset(evm, 0, sizeof(*evm));
    evm->instructions.prev = &evm->instructions;
    evm->instructions.next = &evm->instructions;
  }

  return evm;
}


evm_disassembler_t *evmdisFinalize(evm_disassembler_t *evm) {
  if(evm) {
    evm_disasm_inst_t *inst, *tmp;

    for(inst = evm->instructions.next; inst != &evm->instructions; inst = tmp) {
      tmp = inst->next;
      free(inst);
    }
    memset(evm, 0, sizeof(*evm));
  }

  return evm;
}


void evmdisFree(evm_disassembler_t *evm) {
  free(evm);
}


static void evmdisAddTableEntry(evm_disassembler_t *evm, const uint8_t *bin, int length) {
  evm_disasm_inst_t *inst = evm->instructions.prev;
  int32_t *ptr = NULL;

  switch(length) {
    case 1:
      if(inst->opcode == OP_JTBL) {
        ptr = (int32_t *) &inst->targets[inst->arg.pair[1]++];
      }
      else {
        ptr = (int32_t *) &inst->targets[0];
      }

      *ptr = inst->offset + *(int8_t *) bin;
    break;

    case 2:
      if(inst->opcode == OP_LJTBL) {
        ptr = (int32_t *) &inst->targets[inst->arg.pair[1]++];
      }
      else {
        ptr = (int32_t *) &inst->targets[0];
      }

      *ptr = inst->offset + (((((int32_t) bin[0]) << 16) | (((int32_t) bin[1]) << 24)) >> 16);
    break;

    case 3:
      inst->targets[0] = inst->offset + (((((int32_t) bin[0]) <<  8) |
                                         (((int32_t) bin[1]) << 16) |
                                          (((int32_t) bin[2]) << 24)) >> 8);
    break;
  }
}


#define SIZE_FOR(_cnt) (((sizeof(evm_disasm_inst_t) + 7) & ~7) + (sizeof(uint32_t) * _cnt))

static void evmdisAddInstruction(evm_disassembler_t *evm, const uint8_t *bin, uint32_t off, int len) {
  evm_disasm_inst_t *inst;

  switch(bin[off]) {
    // function calls
    case OP_CALL:
    case OP_LCALL:
    // jump instructions
    case OP_JMP:
    case OP_JLT:
    case OP_JLE:
    case OP_JNE:
    case OP_JEQ:
    case OP_JGE:
    case OP_JGT:
    case OP_LJMP:
    case OP_LJLT:
    case OP_LJLE:
    case OP_LJNE:
    case OP_LJEQ:
    case OP_LJGE:
    case OP_LJGT:
      if((inst = calloc(1, SIZE_FOR(1)))) {
        inst->targets = (uint32_t *) &inst[1];
      }
    break;

    // jump table instructions
    case OP_JTBL:
    case OP_LJTBL:
      if((inst = calloc(1, SIZE_FOR(bin[off + 1] + 1)))) {
        inst->targets = (uint32_t *) &inst[1];
      }
    break;

    default: // simple instructions
      inst = calloc(1, SIZE_FOR(0));
    break;
  }

  if(inst) {
    inst->prev = evm->instructions.prev;
    inst->next = &evm->instructions;
    evm->instructions.prev = inst;
    inst->prev->next = inst;
    inst->offset = off;
    memcpy(&inst->arg.raw[0], &bin[off + 1], len - 1);
    inst->opcode = bin[off];

    if(inst->targets) {
      switch(inst->opcode) {
        // one byte delta
        case OP_JMP:
        case OP_JLT:
        case OP_JLE:
        case OP_JNE:
        case OP_JEQ:
        case OP_JGE:
        case OP_JGT:
          evmdisAddTableEntry(evm, &bin[off + 1], 1);
        break;

        // two bytte delta
        case OP_CALL:
        case OP_LJMP:
        case OP_LJLT:
        case OP_LJLE:
        case OP_LJNE:
        case OP_LJEQ:
        case OP_LJGE:
        case OP_LJGT:
          evmdisAddTableEntry(evm, &bin[off + 1], 2);
        break;

        // three byte delta
        case OP_LCALL:
          evmdisAddTableEntry(evm, &bin[off + 1], 3);
        break;

        default:
          // nothing to do here
        break;
      }
    }
  }
}

#undef SIZE_FOR


static int evmdisResolve(evm_disassembler_t *evm, evm_disasm_inst_t *inst, int entry) {
  uint32_t target = inst->targets[entry];

  if(target > inst->offset) {
    for(inst = inst->next; inst != &evm->instructions; inst = inst->next) {
      if(inst->offset == target) {
        inst->label = -1;
        return 0;
      }
    }
  }
  else {
    for(; inst != &evm->instructions; inst = inst->prev) {
      if(inst->offset == target) {
        inst->label = -1;
        return 0;
      }
    }
  }

  return -1;
}


uint32_t evmdisFromBuffer(evm_disassembler_t *evm, const uint8_t *buffer, uint32_t length) {
  uint32_t consumed = 0;
  int ok = -1;

  if(evm && buffer) {
    int entries = 0;

    while(ok && consumed < length) {
      if(entries) {
        if(evm->instructions.prev->opcode == OP_JTBL) {
          evmdisAddTableEntry(evm, &buffer[consumed], 1);
          consumed += 1;
        }
        else if(evm->instructions.prev->opcode == OP_LJTBL) {
          if((length - consumed) >= 2) {
            evmdisAddTableEntry(evm, &buffer[consumed], 2);
            consumed += 2;
          }
          else {
            ok = 0; // not enough program remaining
          }
        }
        else {
          ok = 0; // not a table based jump
        }
        --entries;
        continue; // don't do normal instruction processing
      }

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
        case OP_SREAD:
        case OP_SWRITE8:
        case OP_SWRITE16:
        case OP_SWRITE24:
        case OP_SWRITE32:
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
          evmdisAddInstruction(evm, buffer, consumed, 1);
          consumed += 1;
        break;

        // opcode + 1
        case OP_JTBL:
        case OP_LJTBL:
          if((length - consumed) >= 2) {
            entries = buffer[consumed + 1] + 1;
          }
          // fallthrough
        case OP_BCALL:
        case OP_PUSH_8I:
        case OP_REM_R:
        case OP_TRUNC:
        case OP_SIGNEXT:
#if EVM_MEMORY_SUPPORT == 1
        case OP_SEG:
#endif
        case OP_JMP:
        case OP_JLT:
        case OP_JLE:
        case OP_JNE:
        case OP_JEQ:
        case OP_JGE:
        case OP_JGT:
        case OP_RET_I:
          if((length - consumed) >= 2) {
            evmdisAddInstruction(evm, buffer, consumed, 2);
            consumed += 2;
          }
          else {
            ok = 0;
          }
        break;

        // opcode + 2
        case OP_CALL:
        case OP_PUSH_16I:
#if EVM_MEMORY_SUPPORT == 1
        case OP_READ:
        case OP_WRITE8:
        case OP_WRITE16:
        case OP_WRITE24:
        case OP_WRITE32:
#endif
        case OP_LJMP:
        case OP_LJLT:
        case OP_LJLE:
        case OP_LJNE:
        case OP_LJEQ:
        case OP_LJGE:
        case OP_LJGT:
          if((length - consumed) >= 3) {
            evmdisAddInstruction(evm, buffer, consumed, 3);
            consumed += 3;
          }
          else {
            ok = 0;
          }
        break;

        // opcode + 3
        case OP_LCALL:
        case OP_PUSH_24I:
#if EVM_MEMORY_SUPPORT == 1
        case OP_LREAD:
        case OP_LWRITE8:
        case OP_LWRITE16:
        case OP_LWRITE24:
        case OP_LWRITE32:
#endif
          if((length - consumed) >= 4) {
            evmdisAddInstruction(evm, buffer, consumed, 4);
            consumed += 4;
          }
          else {
            ok = 0;
          }
        break;

        // opcode + 4
        case OP_PUSH_32I:
#if EVM_FLOAT_SUPPORT == 1
        case OP_PUSH_F:
#endif
          if((length - consumed) >= 5) {
            evmdisAddInstruction(evm, buffer, consumed, 5);
            consumed += 5;
          }
          else {
            ok = 0;
          }

        break;

        default:
          ok = 0;
        break;
      }
    }

    if(ok) {
      evm_disasm_inst_t *inst;
      // locate all the jump targets
      for(inst = evm->instructions.next; ok && inst != &evm->instructions; inst = inst->next) {
        if(inst->targets) {
          int entries, idx;

          if(inst->opcode == OP_JTBL || inst->opcode == OP_LJTBL) {
            entries = (((uint32_t) inst->arg.i8) & 0xFF) + 1;
          }
          else {
            entries = 1;
          }

          for(idx = 0; idx < entries; ++idx) {
            if(evmdisResolve(evm, inst, idx)) {
              ok = 0;
            }
          }
        }
      }
    }
  }

  return ok ? consumed : 0;
}


int evmdisFromFile(evm_disassembler_t *evm, FILE *fp) {
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


static const char *OP_STRINGS[] = {
  // FAM_CALL
  "NOP",     "CALL",    "LCALL",   "BCALL",   "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "YIELD",   "HALT",
  // FAM_PUSH
  "PUSH 0",  "PUSH 1",  "PUSH -1", "PUSH",    "PUSH",    "PUSH",    "PUSH",
#if EVM_FLOAT_SUPPORT == 1
  "PUSH 0.0", "PUSH 1.0", "PUSH -1.0", "PUSH",
#else
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", 
#endif
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",  "SWAP",
  // FAM_POP
  "POP",     "POP 2",   "POP 3",   "POP 4",   "POP 5",   "POP 6",   "POP 7",   "POP 8",
  "REM 1",   "REM 2",   "REM 3",   "REM 4",   "REM 5",   "REM 6",   "REM 7",   "REM",
  // FAM_DUP
  "DUP",     "DUP 2",   "DUP 3",   "DUP 4",   "DUP 5",   "DUP 6",   "DUP 7",   "DUP 8",
  "DUP 9",   "DUP 10",  "DUP 11",  "DUP 12",  "DUP 13",  "DUP 14",  "DUP 15",  "DUP 16",
  // FAM_MATH
  "INC",     "DEC",     "ABS",     "NEG",     "ADD",     "SUB",     "MUL",     "DIV",
#if EVM_FLOAT_SUPPORT == 1
  "INCF",    "DECF",    "ABSF",    "NEGF",    "ADDF",    "SUBF",    "MULF",    "DIVF",
#else
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
#endif
  // FAM_BITS
  "LSH",     "RSH",     "AND",     "OR",      "XOR",     "INV",     "BOOL",    "NOT",
  "TRUNC",   "SIGNEXT",
#if EVM_FLOAT_SUPPORT == 1
  "CNVFI",    "CNVFI 1", "CNVIF",  "CNVIF 1",
#else
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
#endif
  "!INVAL!", "!INVAL!",
  // unused
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  // FAM_MEM
#if EVM_MEMORY_SUPPORT == 1
  "SEG",      "READ",     "WRITE8",   "WRITE16", "WRITE24", "WRITE32",  "LREAD",    "LWRITE8",
  "LWRITE16", "LWRITE24", "LWRITE32", "SREAD",   "SWRITE8", "SWRITE16", "SWRITE24", "SWRITE32",
#else
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
#endif
  // FAM_CMP
  "CMP 0",   "CMP 1",   "CMP -1",  "CMP",
#if EVM_FLOAT_SUPPORT == 1
  "CMPF 0.0", "CMPF 1.0", "CMP -1.0", "CMPF",
#else
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
#endif
  "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!", "!INVAL!",
  // FAM_JMP
  "JMP",     "JLT",     "JLE",     "JNE",     "JEQ",     "JGE",     "JGT",     "JTBL",
  "LJMP",    "LJLT",    "LJLE",    "LJNE",    "LJEQ",    "LJGE",    "LJGT",    "LJTBL",
  // FAM_RET
  "RET",     "RET 1",   "RET 2",   "RET 3",   "RET 4",   "RET 5",   "RET 6",   "RET 7",
  "RET 8",   "RET 9",   "RET 10",  "RET 11",  "RET 12",  "RET 13",  "RET 14",  "RET",
};


static int evmdisStringifyInstructions(const evm_disassembler_t *evm) {
  int result = -1;

  if(evm) {
    evm_disasm_inst_t *inst;
    result = 0;

    // don't do anything if it has already been stringified
    if(!evm->instructions.next->text) {
      for(inst = evm->instructions.next; inst != &evm->instructions; inst = inst->next) {
        int len = inst->label ? snprintf(NULL, 0U, "\nLAB_%06X:\n", inst->offset): 0;

        switch(inst->opcode) {
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
#if EVM_MEMORY_SUPPORT == 1
          case OP_SEG:
          case OP_SREAD:
          case OP_SWRITE8:
          case OP_SWRITE16:
          case OP_SWRITE24:
          case OP_SWRITE32:
#endif
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
            len += snprintf(NULL, 0U, "    %s\n", OP_STRINGS[inst->opcode]);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s\n",
                       inst->offset, OP_STRINGS[inst->opcode]);
            }
            else {
              snprintf(inst->text, len + 1, "    %s\n", OP_STRINGS[inst->opcode]);
            }
          break;

          case OP_JTBL:
          case OP_LJTBL: {
            int entries = inst->arg.i8 + 1, idx;

            len += snprintf(NULL, 0U, "    %s\n", OP_STRINGS[inst->opcode]);
            // determine the jump table length
            for(idx = 0; idx < entries; ++idx) {
              len += snprintf(NULL, 0U, ".addr LAB_%06X\n", inst->targets[idx]);
            }

            inst->text = malloc(len + 1);
            inst->length = len;

            len = inst->label ? snprintf(inst->text, len, "\nLAB_%06X:\n", inst->offset) : 0;

            len += snprintf(&inst->text[len], inst->length - len + 1,
                            "    %s\n", OP_STRINGS[inst->opcode]);
            // print the jump table
            for(idx = 0; idx < entries; ++idx) {
              len += snprintf(&inst->text[len], inst->length - len + 1,
                              ".addr LAB_%06X\n", inst->targets[idx]);
            }
          } break;

          // op + int8
          case OP_PUSH_8I:
            len += snprintf(NULL, 0U, "    %s %d\n", OP_STRINGS[inst->opcode], inst->arg.i8);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s %d\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->arg.i8);
            }
            else {
              snprintf(inst->text, len + 1, "    %s %d\n", OP_STRINGS[inst->opcode], inst->arg.i8);
            }
          break;

          // op + uint8
          case OP_BCALL:
          case OP_TRUNC:
          case OP_SIGNEXT:
          case OP_RET_I:
            len += snprintf(NULL, 0U, "    %s %u\n", OP_STRINGS[inst->opcode], inst->arg.i32 & 0xFF);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s %u\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->arg.i32 & 0xFF);
            }
            else {
              snprintf(inst->text, len + 1, "    %s %u\n",
                       OP_STRINGS[inst->opcode], inst->arg.i32 & 0xFF);
            }
          break;

          // op + 2 nybbles
          case OP_REM_R:
            len += snprintf(NULL, 0U, "    %s %d %d\n", OP_STRINGS[inst->opcode],
                            ((inst->arg.i8 >> 4) & 0x0F) + 1, (inst->arg.i8 & 0x0F) + 1);
            inst->text = malloc(len + 1);
            inst->length = len; // don't count the NUL terminator

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s %d %d\n",
                       inst->offset, OP_STRINGS[inst->opcode],
                       ((inst->arg.i8 >> 4) & 0x0F) + 1, (inst->arg.i8 & 0x0F) + 1);
            }
            else {
              snprintf(inst->text, len + 1, "    %s %d %d\n", OP_STRINGS[inst->opcode],
                       ((inst->arg.i8 >> 4) & 0x0F) + 1, (inst->arg.i8 & 0x0F) + 1);
            }
          break;

          // op + label
          case OP_JMP:
          case OP_JLT:
          case OP_JLE:
          case OP_JNE:
          case OP_JEQ:
          case OP_JGE:
          case OP_JGT:
          case OP_CALL:
          case OP_LJMP:
          case OP_LJLT:
          case OP_LJLE:
          case OP_LJNE:
          case OP_LJEQ:
          case OP_LJGE:
          case OP_LJGT:
          case OP_LCALL:
            len += snprintf(NULL, 0U, "    %s LAB_%06X\n",
                            OP_STRINGS[inst->opcode], inst->targets[0]);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s LAB_%06X\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->targets[0]);
            }
            else {
              snprintf(inst->text, len + 1, "    %s LAB_%06X\n",
                       OP_STRINGS[inst->opcode], inst->targets[0]);
            }
          break;

          // op + int16
          case OP_PUSH_16I:
            len += snprintf(NULL, 0U, "    %s %d\n", OP_STRINGS[inst->opcode], inst->arg.i16);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s %d\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->arg.i16);
            }
            else {
              snprintf(inst->text, len + 1, "    %s %d\n", OP_STRINGS[inst->opcode], inst->arg.i16);
            }
          break;

#if EVM_MEMORY_SUPPORT == 1
          // op + uint16/uint24
          case OP_READ:
          case OP_WRITE8:
          case OP_WRITE16:
          case OP_WRITE24:
          case OP_WRITE32:
          case OP_LREAD:
          case OP_LWRITE8:
          case OP_LWRITE16:
          case OP_LWRITE24:
          case OP_LWRITE32:
            len += snprintf(NULL, 0U, "    %s 0x%X\n", OP_STRINGS[inst->opcode], inst->arg.i32);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s 0x%X\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->arg.i32);
            }
            else {
              snprintf(inst->text, len + 1, "    %s 0x%X\n", OP_STRINGS[inst->opcode], inst->arg.i32);
            }
          break;
#endif

          // op + int24/int32
          case OP_PUSH_24I:
          case OP_PUSH_32I:
            len += snprintf(NULL, 0U, "    %s %d\n", OP_STRINGS[inst->opcode], inst->arg.i32);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s %d\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->arg.i32);
            }
            else {
              snprintf(inst->text, len + 1, "    %s %d\n", OP_STRINGS[inst->opcode], inst->arg.i32);
            }
          break;

#if EVM_FLOAT_SUPPORT == 1
          // op + float
          case OP_PUSH_F:
            len += snprintf(NULL, 0U, "    %s %f\n", OP_STRINGS[inst->opcode], inst->arg.f32);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s %f\n",
                       inst->offset, OP_STRINGS[inst->opcode], inst->arg.f32);
            }
            else {
              snprintf(inst->text, len + 1, "    %s %f\n", OP_STRINGS[inst->opcode], inst->arg.f32);
            }
          break;
#endif

          default:
            len += snprintf(NULL, 0U, "    %s\n", OP_STRINGS[inst->opcode]);
            inst->text = malloc(len + 1);
            inst->length = len;

            if(inst->label) {
              snprintf(inst->text, len + 1, "\nLAB_%06X:\n    %s\n",
                       inst->offset, OP_STRINGS[inst->opcode]);
            }
            else {
              snprintf(inst->text, len + 1, "    %s\n", OP_STRINGS[inst->opcode]);
            }
          break;
        }
      }
    }
  }

  return result;
}


int evmdisToBuffer(const evm_disassembler_t *evm, char **buf, int *len) {
  int result = -1;

  if(len) {
    result = evmdisStringifyInstructions(evm);

    if(!result) {
      int length = snprintf(NULL, 0U, ".name MAIN\n.offset 0\n\n") - 1;
      evm_disasm_inst_t *inst;

      for(inst = evm->instructions.next; inst != &evm->instructions; inst = inst->next) {
        length += inst->length;
      }

      *len = length; // don't report the NUL terminator as part of the buffer

      if(buf) {
        if((*buf = malloc(length + 1))) { // include a NUL terminator
          char *ptr = *buf;
          int count = snprintf(ptr, length, ".name MAIN\n.offset 0\n\n");
          ptr += count;

          for(inst = evm->instructions.next; inst != &evm->instructions; inst = inst->next) {
            strcpy(ptr, inst->text);
            length -= inst->length;
            ptr += inst->length;
          }
        }
        else {
          result = -1;
        }
      }
    }
  }

  return result;
}


int evmdisToFile(const evm_disassembler_t *evm, FILE *dst) {
  int result = -1;

  if(evm && dst) {
    char *buffer = NULL;
    int length = 0;

    if(!(result = evmdisToBuffer(evm, &buffer, &length)) && length > 0) {
      fprintf(dst, "%s", buffer);
      result = 0;
    }

    if(buffer) {
      free(buffer);
    }
  }

  return result;
}

