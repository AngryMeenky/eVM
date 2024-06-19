#include "evm.h"
#include "evm/opcodes.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define EVM_CALLOC(NUM, SZ) calloc((NUM), (SZ))
#define EVM_MALLOC(SZ)      malloc(SZ)
#define EVM_FREE(PTR)       free(PTR)


evm_t *evmAllocate() {
  evm_t *retVal;
  EVM_TRACE("Enter " __FUNCTION__);
  retVal = (evm_t *) EVM_CALLOC(1, sizeof(evm_t));
  EVM_TRACE("Exit " __FUNCTION__);
  return retVal;
}


evm_t *evmInitialize(evm_t *vm, void *user, uint16_t stackSize) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    (void) evmFinalize(vm);
    vm->maxStack = stackSize;
    vm->stack = (int32_t *) EVM_CALLOC(stackSize, sizeof(int32_t));
    vm->env = user;
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return vm;
}


void *evmFinalize(evm_t *vm) {
  void *retVal = NULL;
  EVM_TRACE("Enter " __FUNCTION__);

  if(vm) {
    if(vm->stack  ) { EVM_FREE((void *) vm->stack);   }
    if(vm->program) { EVM_FREE((void *) vm->program); }

    retVal = vm->env;

    memset(vm, 0, sizeof(evm_t));
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return retVal;
}


void evmFree(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  evmFinalize(vm);
  EVM_FREE((void *) vm);
  EVM_TRACE("Exit " __FUNCTION__);
}


int evmSetProgram(evm_t *vm, uint8_t *prog, uint32_t length) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm && prog && length < 0xFFFFFFFFU) {
    if(vm->program) { EVM_FREE((void *) vm->program); }
    vm->maxProgram = length;
    vm->program = EVM_MALLOC(length + 1U);
    memcpy((void *) vm->program, prog, length);
    vm->program[length] = OP_HALT; // halt terminate the program

    EVM_TRACE("Exit " __FUNCTION__);
    return 0;
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1; // failure
}


int32_t evmUnboundHandler(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    EVM_WARNF("Called unbound builtin @ %08X", vm->ip);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return 0;
}


int32_t evmIllegalStateHandler(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    vm->flags |= EVM_HALTED;
    EVM_ERRORF(
      "Illegal state: sp(%04X/%04X) ip(%08X/%08X), flags(%08X)\n"
      "                      stack(%p), prog(%p), env(%p)",
      vm->sp, vm->maxStack, vm->ip, vm->maxProgram, vm->flags,
      vm->stack, vm->program, vm->env
    );
  }

  EVM_FATAL("Unrecoverable state");
  return -1;
}


int32_t evmStackOverflowHandler(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    vm->flags |= EVM_HALTED;
    EVM_ERRORF("Stack overflow: sp(%04X) ip(%08X)", vm->sp, vm->ip);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1;
}


int32_t evmStackUnderflowHandler(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    vm->flags |= EVM_HALTED;
    EVM_ERRORF("Stack underflow: sp(%04X) ip(%08X)", vm->sp, vm->ip);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1;
}


int32_t evmIllegalInstructionHandler(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    vm->flags |= EVM_HALTED;
    EVM_ERRORF("Illegal instruction: %02X @ %08X", vm->program[vm->ip], vm->ip);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1;
}


int evmHasHalted(const evm_t *vm) {
  int result;
  EVM_TRACE("Enter " __FUNCTION__);
  result = vm ? (vm->flags & EVM_HALTED) == EVM_HALTED : -1;
  EVM_TRACE("Exit " __FUNCTION__);
  return result;
}


#define EVM_PUSH(VM, VAL) \
  do { \
    if(((VM).sp + 1) < (VM).maxStack || !EVM_BUILTINS[EVM_STACK_OVERFLOW](&(VM))) { \
      typeof(VAL) _val = (VAL); \
      (VM).stack[(VM).sp++] = *(typeof(*(VM).stack) *) &_val; \
    } \
    else { \
      (void) EVM_BUILTINS[EVM_ILLEGAL_STATE](&(VM));\
    } \
  } while(0)


#define EVM_POP(VM, COUNT) \
  do { \
    if((VM).sp < ((VM).sp - (typeof((VM).sp)) ((COUNT) + 1))) { \
      (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&(VM)); \
    } \
    else { \
      (VM).sp -= (typeof((VM).sp)) (COUNT); \
    } \
  } while(0)


#define EVM_REMOVE(VM, DEPTH, COUNT) \
  do { \
    typeof((VM).sp) _d = (typeof((VM).sp)) (DEPTH); \
    typeof((VM).sp) _c = (typeof((VM).sp)) (COUNT); \
    if((VM).sp < ((VM).sp - (_d + _c - 1))) { \
      (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&(VM)); \
    } \
    else { \
      memmove(&(VM).stack[(VM).sp - (_d + _c)], \
              &(VM).stack[(VM).sp -  _d      ], \
              (_d) * sizeof(*(VM).stack)); \
      (VM).sp -= _c; \
    } \
  } while(0)


#define EVM_DUP(VM, DEPTH) \
  do { \
    if((VM).sp < ((VM).sp - (typeof((VM).sp)) ((DEPTH) + 1))) { \
      (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&(VM)); \
    } \
    else { \
      EVM_PUSH(VM, (VM).stack[(VM).sp - (DEPTH)]); \
    } \
  } while(0)


#define EVM_BIN_OP_I(VM, OP) \
  do { \
    if(local.sp < 2U) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); } \
    else { \
      EVM_STACK_I(local, 1) = EVM_TOP_I(local) OP EVM_STACK_I(local, 1); \
      --local.sp; \
    } \
  } while(0)


#define EVM_BIN_OP_F(VM, OP) \
  do { \
    if(local.sp < 2U) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); } \
    else { \
      EVM_STACK_F(local, 1) = EVM_TOP_F(local) OP EVM_STACK_F(local, 1); \
      --local.sp; \
    } \
  } while(0)


#define EVM_STACK_I(VM, DEPTH) ((VM).stack[(VM).sp - ((DEPTH) + 1U)])
#define EVM_STACK_FP(VM, DEPTH) ((float *) &EVM_STACK_I(VM, DEPTH))
#define EVM_STACK_F(VM, DEPTH) (*EVM_STACK_FP(VM, DEPTH))

#define EVM_TOP_I(VM) EVM_STACK_I(VM, 0U)
#define EVM_TOP_FP(VM) EVM_STACK_FP(VM, 0U)
#define EVM_TOP_F(VM) EVM_STACK_F(VM, 0U)


static int32_t evmLoadInt8(const uint8_t *src) {
  return (int32_t) *(const int8_t *) src;
}


static int32_t evmLoadInt16(const uint8_t *src) {
#if EVM_UNALIGNED_READS == 1
  return (int32_t) *(const int16_t *) src;
#else
  return (int32_t) (((int16_t) src[0]) | (((int16_t) src[1]) << 8));
#endif
}


static int32_t evmLoadInt24(const uint8_t *src) {
  return ((int32_t) ((((uint32_t) src[0]) <<  8) | (((uint32_t) src[1]) << 16) |
                    (((uint32_t) src[2]) << 24))) >> 8;
}


static int32_t evmLoadInt32(const uint8_t *src) {
#if EVM_UNALIGNED_READS == 1
  return (int32_t) *(const int32_t *) src;
#else
  return  ((int32_t) src[0]      ) | (((int32_t) src[1]) <<  8) |
          ((int32_t) src[2] << 16) | (((int32_t) src[3]) << 24);
#endif
}


int evmRun(evm_t *vm, uint32_t maxOps) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm && vm->program) {
    evm_t local = *vm; // copy the state back to a local eVM
    uint32_t ops = 0;

    EVM_DEBUGF("Running VM for %u operations", maxOps);
    while(ops++ < maxOps && (local.flags & EVM_HALTED) == 0) {
      switch(local.program[local.ip]) {
        case OP_NOP:
          ++local.ip; // do nothing except move to the next instruction
        break;

        case OP_CALL: {
          EVM_PUSH(local, local.ip + 3U); // push the return instruction pointer
          // update the instruction pointer to the function
          local.ip += evmLoadInt16(&local.program[local.ip + 1]);
        } break;

        case OP_LCALL:
          EVM_PUSH(local, local.ip + 4U); // push the return instruction pointer
          // update the instruction pointer to the function
          local.ip = evmLoadInt24(&local.program[local.ip + 1]);
        break;

        case OP_BCALL: {
          uint8_t id = local.program[local.ip + 1U];
#if EVM_MAX_BUILTINS != 256
          if(id >= EVM_MAX_BUILTINS) {
            (void) EVM_BUILTINS[EVM_ILLEGAL_INSTRUCTION](&local);
          }
          else {
#endif
          local.ip += 2; // move to the next instruction, allow builtin to override on error
          EVM_PUSH(local, EVM_BUILTINS[id](&local)); 
#if EVM_MAX_BUILTINS != 256
          }
#endif
        } break;

        case OP_HALT:
          EVM_INFOF("Halting @ %08X", vm->ip);
          local.flags |= EVM_HALTED;
        break;

        case OP_PUSH_I0:
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 0); // push a zero onto the stack
        break;

        case OP_PUSH_I1:
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 1); // push a one onto the stack
        break;

        case OP_PUSH_IN1:
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, -1); // push a negative one onto the stack
        break;

        case OP_PUSH_8I:
          local.ip += 2U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt8(&local.program[local.ip - 1U])); // push a signed byte
        break;

        case OP_PUSH_16I:
          local.ip += 3U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt16(&local.program[local.ip - 2U])); // push a signed short
        break;

        case OP_PUSH_24I:
          local.ip += 4U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt24(&local.program[local.ip - 3U])); // push a signed int24
        break;

        case OP_PUSH_32I:
          local.ip += 5U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt32(&local.program[local.ip - 4U])); // push a signed int
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_PUSH_F0:
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 0.0f); // push a zero onto the stack
        break;

        case OP_PUSH_F1:
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 1.0f); // push a one onto the stack
        break;

        case OP_PUSH_FN1:
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, -1.0f); // push a negative one onto the stack
        break;

        case OP_PUSH_F:
          local.ip += 5U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt32(&local.program[local.ip - 4U])); // push a float
        break;
#endif

        case OP_SWAP:
          ++local.ip; // move to the next instruction
          if(local.sp < 2) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            uint32_t tmp = local.stack[local.sp - 1U];
            local.stack[local.sp - 1U] = local.stack[local.sp - 2U];
            local.stack[local.sp - 2U] = tmp;
          }
        break;

        case OP_POP_1:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 1U); // remove the top of the stack
        break;

        case OP_POP_2:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 2U); // remove the top two values from the stack
        break;

        case OP_POP_3:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 3U); // remove the top three values from the stack
        break;

        case OP_POP_4:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 4U); // remove the top four values from the stack
        break;

        case OP_POP_5:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 5U); // remove the top five values from the stack
        break;

        case OP_POP_6:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 6U); // remove the top six values from the stack
        break;

        case OP_POP_7:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 7U); // remove the top seven values from the stack
        break;

        case OP_POP_8:
          ++local.ip; // move to the next instruction
          EVM_POP(local, 8U); // remove the top eight values from the stack
        break;

        case OP_REM_1:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 1U, 1U); // remove second value from the stack
        break;

        case OP_REM_2:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 2U, 1U); // remove third value from the stack
        break;

        case OP_REM_3:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 3U, 1U); // remove fourth value from the stack
        break;

        case OP_REM_4:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 4U, 1U); // remove fifth value from the stack
        break;

        case OP_REM_5:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 5U, 1U); // remove sixth value from the stack
        break;

        case OP_REM_6:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 6U, 1U); // remove seventh value from the stack
        break;

        case OP_REM_7:
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 7U, 1U); // remove eighth value from the stack
        break;

        case OP_REM_R:
          local.ip += 2; // move to the next instruction
          // remove up 16 values from a depth of up to 16
          EVM_REMOVE(local, (local.program[local.ip - 1] >>    4) + 1U,
                            (local.program[local.ip - 1] &  0x0F) + 1U);
        break;

        case OP_DUP_0:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 1U); // duplicate the top stack value
        break;

        case OP_DUP_1:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 2U); // duplicate the second value in the stack
        break;

        case OP_DUP_2:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 3U); // duplicate the third value in the stack
        break;

        case OP_DUP_3:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 4U); // duplicate the fourth value in the stack
        break;

        case OP_DUP_4:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 5U); // duplicate the fifth value in the stack
        break;

        case OP_DUP_5:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 6U); // duplicate the sixth value in the stack
        break;

        case OP_DUP_6:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 7U); // duplicate the seventh value in the stack
        break;

        case OP_DUP_7:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 8U); // duplicate the eighth value in the stack
        break;

        case OP_DUP_8:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 9U); // duplicate the nineth value in the stack
        break;

        case OP_DUP_9:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 10U); // duplicate the tenth value in the stack
        break;

        case OP_DUP_10:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 11U); // duplicate the eleventh value in the stack
        break;

        case OP_DUP_11:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 12U); // duplicate the twelfth value in the stack
        break;

        case OP_DUP_12:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 13U); // duplicate the thirteenth value in the stack
        break;

        case OP_DUP_13:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 14U); // duplicate the fourteenth value in the stack
        break;

        case OP_DUP_14:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 15U); // duplicate the fifteenth value in the stack
        break;

        case OP_DUP_15:
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 16U); // duplicate the sixteenth value in the stack
        break;

        case OP_INC_I:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { ++EVM_TOP_I(local); } // increment the value on top of the stack
        break;

        case OP_DEC_I:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { --EVM_TOP_I(local); } // decrement the value on top of the stack
        break;

        case OP_ABS_I:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_I(local) = abs(EVM_TOP_I(local)); } // absolute value the top of the stack
        break;

        case OP_NEG_I:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_I(local) = -EVM_TOP_I(local); } // negate the top of the stack
        break;

        case OP_ADD_I:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, +); // replace the top two values with their sum
        break;

        case OP_SUB_I:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, -); // replace the top two values with their difference
        break;

        case OP_MUL_I:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, *); // replace the top two values with their product
        break;

        case OP_DIV_I:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, /); // replace the top two values with their quotient
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_INC_F:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_F(local) += 1.0f; } // increment the value on top of the stack
        break;

        case OP_DEC_F:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_F(local) -= 1.0f; } // decrement the value on top of the stack
        break;

        case OP_ABS_F:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_F(local) = fabs(EVM_TOP_F(local)); } // absolute value the top of the stack
        break;

        case OP_NEG_F:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_F(local) = -EVM_TOP_F(local); } // negate the top of the stack
        break;

        case OP_ADD_F:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, +); // replace the top two values with their sum
        break;

        case OP_SUB_F:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, -); // replace the top two values with their difference
        break;

        case OP_MUL_F:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, *); // replace the top two values with their product
        break;

        case OP_DIV_F:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, /); // replace the top two values with their quotient
        break;
#endif

        case OP_LSH:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, <<);
        break;

        case OP_RSH:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, >>);
        break;

        case OP_AND:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, &);
        break;

        case OP_OR:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, |);
        break;

        case OP_XOR:
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, ^);
        break;

        case OP_INV:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_I(local) = ~EVM_TOP_I(local); }
        break;

        case OP_BOOL:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_I(local) = !!EVM_TOP_I(local); }
        break;

        case OP_NOT:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_I(local) = !EVM_TOP_I(local); }
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_CONV_FI:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_I(local) = (int32_t) EVM_TOP_F(local); }
        break;

        case OP_CONV_FI_0:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_STACK_I(local, 1U) = (int32_t) EVM_STACK_F(local, 1U); }
        break;

        case OP_CONV_IF:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_TOP_F(local) = (float) EVM_TOP_I(local); }
        break;

        case OP_CONV_IF_0:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else { EVM_STACK_F(local, 1U) = (float) EVM_STACK_I(local, 1U); }
        break;
#endif

        case OP_CMP_I0:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            int32_t val = EVM_TOP_I(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 0) {       local.flags |= EVM_LESS;    }
            else if(val == 0) { local.flags |= EVM_EQUAL;   }
            else {              local.flags |= EVM_GREATER; }
          }
        break;

        case OP_CMP_I1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            int32_t val = EVM_TOP_I(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 1) {       local.flags |= EVM_LESS;    }
            else if(val == 1) { local.flags |= EVM_EQUAL;   }
            else {              local.flags |= EVM_GREATER; }
          }
        break;

        case OP_CMP_IN1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            int32_t val = EVM_TOP_I(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < -1) {       local.flags |= EVM_LESS;    }
            else if(val == -1) { local.flags |= EVM_EQUAL;   }
            else {               local.flags |= EVM_GREATER; }
          }
        break;

        case OP_CMP_I:
          ++local.ip; // move to the next instruction
          if(local.sp < 2U) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            int32_t lhs = EVM_TOP_I(local);
            int32_t rhs = EVM_STACK_I(local, 1U);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(lhs < rhs) {       local.flags |= EVM_LESS;    }
            else if(lhs == rhs) { local.flags |= EVM_EQUAL;   }
            else {                local.flags |= EVM_GREATER; }
          }
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_CMP_F0:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            float val = EVM_TOP_F(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 0.0f) {       local.flags |= EVM_LESS;    }
            else if(val == 0.0f) { local.flags |= EVM_EQUAL;   }
            else {                 local.flags |= EVM_GREATER; }
          }
        break;

        case OP_CMP_F1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            float val = EVM_TOP_F(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 1.0f) {       local.flags |= EVM_LESS;    }
            else if(val == 1.0f) { local.flags |= EVM_EQUAL;   }
            else {                 local.flags |= EVM_GREATER; }
          }
        break;

        case OP_CMP_FN1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            float val = EVM_TOP_F(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < -1.0f) {       local.flags |= EVM_LESS;    }
            else if(val == -1.0f) { local.flags |= EVM_EQUAL;   }
            else {                  local.flags |= EVM_GREATER; }
          }
        break;

        case OP_CMP_F:
          ++local.ip; // move to the next instruction
          if(local.sp < 2U) { (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local); }
          else {
            float lhs = EVM_TOP_F(local);
            float rhs = EVM_STACK_F(local, 1U);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(lhs < rhs) {       local.flags |= EVM_LESS;    }
            else if(lhs == rhs) { local.flags |= EVM_EQUAL;   }
            else {                local.flags |= EVM_GREATER; }
          }
        break;
#endif

        case OP_JMP:
          local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
        break;

        case OP_JLT:
          if(local.flags & EVM_LESS) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JLE:
          if(local.flags & (EVM_LESS | EVM_EQUAL)) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JNE:
          if(local.flags & (EVM_LESS | EVM_GREATER)) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JEQ:
          if(local.flags & EVM_EQUAL) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JGE:
          if(local.flags & (EVM_GREATER | EVM_EQUAL)) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JGT:
          if(local.flags & EVM_GREATER) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJMP:
          local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
        break;

        case OP_LJLT:
          if(local.flags & EVM_LESS) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJLE:
          if(local.flags & (EVM_LESS | EVM_EQUAL)) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJNE:
          if(local.flags & (EVM_LESS | EVM_GREATER)) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJEQ:
          if(local.flags & EVM_EQUAL) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJGE:
          if(local.flags & (EVM_GREATER | EVM_EQUAL)) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJGT:
          if(local.flags & EVM_GREATER) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JTBL:
          if(!local.sp) {
            (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local);
          }
          else {
            local.ip += evmLoadInt8(&local.program[EVM_TOP_I(local) + local.ip + 1U]);
          }
        break;

        case OP_LJTBL:
          if(!local.sp) {
            (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local);
          }
          else {
            local.ip += evmLoadInt16(&local.program[EVM_TOP_I(local) * 2 + local.ip + 1U]);
          }
        break;

        case OP_RET:
          if(!local.sp) {
            (void) EVM_BUILTINS[EVM_STACK_UNDERFLOW](&local);
          }
          else {
            local.ip = (uint32_t) EVM_TOP_I(local);
            --local.sp;
          }
        break;

        case OP_RET_1:
          local.ip = (uint32_t) EVM_STACK_I(local, 1U);
          EVM_REMOVE(local, 1U, 1U); // remove return address from the stack
        break;

        case OP_RET_2:
          local.ip = (uint32_t) EVM_STACK_I(local, 2U);
          EVM_REMOVE(local, 2U, 1U); // remove return address from the stack
        break;

        case OP_RET_3:
          local.ip = (uint32_t) EVM_STACK_I(local, 3U);
          EVM_REMOVE(local, 3U, 1U); // remove return address from the stack
        break;

        case OP_RET_4:
          local.ip = (uint32_t) EVM_STACK_I(local, 4U);
          EVM_REMOVE(local, 4U, 1U); // remove return address from the stack
        break;

        case OP_RET_5:
          local.ip = (uint32_t) EVM_STACK_I(local, 5U);
          EVM_REMOVE(local, 5U, 1U); // remove return address from the stack
        break;

        case OP_RET_6:
          local.ip = (uint32_t) EVM_STACK_I(local, 6U);
          EVM_REMOVE(local, 6U, 1U); // remove return address from the stack
        break;

        case OP_RET_7:
          local.ip = (uint32_t) EVM_STACK_I(local, 7U);
          EVM_REMOVE(local, 7U, 1U); // remove return address from the stack
        break;

        case OP_RET_8:
          local.ip = (uint32_t) EVM_STACK_I(local, 8U);
          EVM_REMOVE(local, 8U, 1U); // remove return address from the stack
        break;

        case OP_RET_9:
          local.ip = (uint32_t) EVM_STACK_I(local, 9U);
          EVM_REMOVE(local, 9U, 1U); // remove return address from the stack
        break;

        case OP_RET_10:
          local.ip = (uint32_t) EVM_STACK_I(local, 10U);
          EVM_REMOVE(local, 10U, 1U); // remove return address from the stack
        break;

        case OP_RET_11:
          local.ip = (uint32_t) EVM_STACK_I(local, 11U);
          EVM_REMOVE(local, 11U, 1U); // remove return address from the stack
        break;

        case OP_RET_12:
          local.ip = (uint32_t) EVM_STACK_I(local, 12U);
          EVM_REMOVE(local, 12U, 1U); // remove return address from the stack
        break;

        case OP_RET_13:
          local.ip = (uint32_t) EVM_STACK_I(local, 13U);
          EVM_REMOVE(local, 13U, 1U); // remove return address from the stack
        break;

        case OP_RET_14:
          local.ip = (uint32_t) EVM_STACK_I(local, 14U);
          EVM_REMOVE(local, 14U, 1U); // remove return address from the stack
        break;

        case OP_RET_I: {
          uint32_t depth = local.program[local.ip + 1U];
          local.ip = (uint32_t) EVM_STACK_I(local, depth);
          EVM_REMOVE(local, depth, 1U); // remove return address from the stack
        } break;

        default:
          // invoke illegal instruction handler
          (void) EVM_BUILTINS[EVM_ILLEGAL_INSTRUCTION](&local);
        break;
      }
    }
    EVM_DEBUGF("Performed %u of %u VM operations", ops, maxOps);

    *vm = local; // copy the state back to the canonical eVM
    EVM_TRACE("Exit " __FUNCTION__);
    return !!(local.flags & EVM_HALTED);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1;
}

