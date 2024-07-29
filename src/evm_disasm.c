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


static void evmdisAddTableEntry(evm_disassembler_t *evm, uint8_t *bin, int length) {
  evm_disasm_inst_t *inst = evm->instructions->prev;
  int32_t *ptr = NULL;

  switch(length) {
    case 1:
      if(inst->opcode == OP_JTBL) {
        ptr = &inst->targets[inst->pair[1]++];
      }
      else {
        ptr = &inst->targets[0];
      }

      *ptr = inst->offset + *(int8_t *) bin;
    break;

    case 2:
      if(inst->opcode == OP_LJTBL) {
        ptr = (int32_t *) &inst->targets[inst->pair[1]++];
      }
      else {
        ptr = (int32_t *) &inst->targets[0];
      }

      *ptr = inst->offset + ((((int32_t) bin[0]) << 16) | (((int32_t) bin[1]) << 24)) >> 16;
    break;

    case 3:
      &inst->targets[0] = inst->offset + ((((int32_t) bin[0]) <<  8) |
                                          (((int32_t) bin[1]) << 16) |
                                          (((int32_t) bin[2]) << 24)) >> 8;
    break;
  }
}


#define SIZE_FOR(_cnt) (((sizeof(evm_disasm_inst_t) + 7) & ~7) + (sizeof(uint32_t) * _cnt))

static void evmdisAddInstruction(evm_disassembler_t *evm, uint8_t *bin, uint32_t off, int len) {
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
    inst->prev = evm->instructions->prev;
    inst->next = &evm->instructions;
    evm->instructions->prev = inst;
    inst->prev->next = inst;
    inst->offset = off;
    memcpy(&inst->raw[0], &bin[off + 1], len - 1);
    inst->opcode = bin[off];
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


uint32_t evmdisFromBuffer(evm_disassembler_t *evm, uint8_t *buffer, uint32_t length) {
  uint32_t consumed = 0;

  if(evm && buffer) {
    int ok = -1;
    int entries = 0;

    while(ok && consumed < length) {
      if(entries) {
        if(evm->instructions->prev->opcode == OP_JTBL) {
          evmdisAddTableEntry(evm, &buffer[consumed], 1);
          consumed += 1;
        }
        else if(evm->instructions->prev->opcode == OP_LJTBL) {
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
      for(inst = evm->instructions->next; ok && inst != &evm->instructions; inst->next) {
        if(inst->targets) {
          evm_disasm_inst_t *inst;
          int entries, idx;

          if(inst->opcode == OP_JTBL || inst->opcode == OP_LJTBL) {
            entries = inst->i8 + 1, idx;
          }
          else {
            entries = 1;
          }

          for(idx = 0; idx < entries; ++idx) {
            if(evmdisResolve(evm, &inst->targets[idx], inst, idx)) {
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


static const char *JUMP_OPS[] = { "jmp", "jlt", "jle", "jne", "jeq", "jge", "jgt" };


int evmdisToFile(const evm_disassembler_t *evm, FILE *dst) {
  int result = -1;

  if(evm && dst) {
    evm_disasm_inst_t *inst;

    for(inst = evm->instructions->next; inst != &evm->instructions; inst = inst->next) {
      if(inst->label) {
        printf("\nLAB_%06X:\n", inst->offset);
      }


      if(inst->opcode == OP_JTBL || inst->opcode == OP_LJTBL) {
        int entries = inst->i8 + 1, idx;

        printf("    %sjtbl\n", inst->opcode == OP_LJTBL ? "l" : "");
        // print the jump table
        for(idx = 0; idx < entries; ++idx) {
          printf(".addr LAB_%06X\n", inst->targets[idx]);
        }
      }
      else if((inst->opcode & 0xF0) == FAM_JMP) {
        // print instruction and target
        printf("    %s%s LAB_%06X\n",
               inst->opcode & 0x08 ? "l" : "", JUMP_OPS[inst->opcode & 0x07], inst->targets[0]);
      }
      else if(inst->opcode == OP_CALL || inst->opcode == OP_LCALL) {
        // print instruction and target
        printf("    %scall LAB_%06X\n", inst->opcode == OP_LCALL ? "l" : "", inst->targets[0]);
      }
      else {
        // print the instruction with operands
      }
    }
  }

  return result;
}

