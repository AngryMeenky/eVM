#define EVM_IMPL

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
  EVM_DEBUGF("eVM(%p)", retVal);
  EVM_TRACE("Exit " __FUNCTION__);
  return retVal;
}


#if EVM_STATIC_STACK == 1
evm_t *evmInitialize(evm_t *vm, void *user, int32_t *stack, uint16_t stackSize);
#else
evm_t *evmInitialize(evm_t *vm, void *user, uint16_t stackSize) {
#endif
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    (void) evmFinalize(vm);
    vm->maxStack = stackSize;
#if EVM_STATIC_STACK == 1
    vm->stack = stack;
#else
    vm->stack = (int32_t *) EVM_CALLOC(stackSize, sizeof(int32_t));
#endif
    vm->env = user;
    EVM_DEBUGF("eVM(%p) { stack: %p user: %p prog: %p }", vm, vm->stack, vm->env, vm->program);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return vm;
}


void *evmFinalize(evm_t *vm) {
  void *retVal = NULL;
  EVM_TRACE("Enter " __FUNCTION__);

  if(vm) {
    EVM_DEBUGF("eVM(%p) { stack: %p user: %p prog: %p }", vm, vm->stack, vm->env, vm->program);
#if EVM_STATIC_STACK == 0
    if(vm->stack  ) { EVM_FREE((void *) vm->stack);   }
#endif
    if(vm->program) { EVM_FREE((void *) vm->program); }

    retVal = vm->env;

    memset(vm, 0, sizeof(evm_t));
    vm->flags |= EVM_HALTED;
    EVM_DEBUGF("eVM(%p) { stack: %p user: %p prog: %p }", vm, vm->stack, vm->env, vm->program);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return retVal;
}


void evmFree(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  EVM_DEBUGF("eVM(%p) { stack: %p user: %p prog: %p }", vm, vm->stack, vm->env, vm->program);
  EVM_FREE((void *) vm);
  EVM_TRACE("Exit " __FUNCTION__);
}


int evmSetProgram(evm_t *vm, uint8_t *prog, uint32_t length) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm && prog && length < 0xFFFFFFFFU) {
    EVM_DEBUGF("eVM(%p) { stack: %p user: %p prog: %p }", vm, vm->stack, vm->env, vm->program);
#if EVM_STATIC_PROGRAM == 1
    vm->program = prog;
#else
    if(vm->program) { EVM_FREE((void *) vm->program); }
    vm->program = EVM_MALLOC(length + 1U);
    memcpy((void *) vm->program, prog, length);
    vm->program[length] = OP_HALT; // halt terminate the program
#endif
    vm->maxProgram = length;
    vm->flags &= ~EVM_HALTED; // clear the halted flag on success

    EVM_DEBUGF("eVM(%p) { stack: %p user: %p prog: %p }", vm, vm->stack, vm->env, vm->program);
    EVM_TRACE("Exit " __FUNCTION__);
    return 0;
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1; // failure
}


int32_t evmUnboundHandler(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    EVM_WARNF("Called unbound builtin @ %08X", vm->ip - 2U);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return 0;
}


static int32_t evmIllegalState(evm_t *vm) {
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


static int32_t evmStackOverflow(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    vm->flags |= EVM_HALTED;
    EVM_ERRORF("Stack overflow: sp(%04X) ip(%08X)", vm->sp, vm->ip);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1;
}


static int32_t evmStackUnderflow(evm_t *vm) {
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    vm->flags |= EVM_HALTED;
    EVM_ERRORF("Stack underflow: sp(%04X) ip(%08X)", vm->sp, vm->ip);
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return -1;
}


static int32_t evmIllegalInstruction(evm_t *vm) {
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
  result = vm ? (vm->flags & EVM_HALTED) == (uint32_t) EVM_HALTED : -1;
  EVM_TRACE("Exit " __FUNCTION__);
  return result;
}


int evmHasYielded(const evm_t *vm) {
  int result;
  EVM_TRACE("Enter " __FUNCTION__);
  result = vm ? (vm->flags & EVM_YIELD) == EVM_YIELD : -1;
  EVM_TRACE("Exit " __FUNCTION__);
  return result;
}


int evmPush(evm_t *vm, int32_t val) {
  int result = 0;
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    if(vm->sp < vm->maxStack) {
      vm->stack[vm->sp++] = val;
    }
    else {
      result = evmStackOverflow(vm);
    }
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return result;
}


#if EVM_FLOAT_SUPPORT == 1
int evmPushf(evm_t *vm, float val) {
  int result = 0;
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    if(vm->sp < vm->maxStack) {
      vm->stack[vm->sp++] = *(int32_t *) &val;
    }
    else {
      result = evmStackOverflow(vm);
    }
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return result;
}
#endif


int evmPop(evm_t *vm) {
  int result = 0;
  EVM_TRACE("Enter " __FUNCTION__);
  if(vm) {
    if(vm->sp > 0) {
      vm->sp--;
    }
    else {
      result = evmStackUnderflow(vm);
    }
  }

  EVM_TRACE("Exit " __FUNCTION__);
  return result;
}


#define EVM_PUSH(VM, VAL) \
  do { \
    if(((VM).sp + 1) < (VM).maxStack || !evmStackOverflow(&(VM))) { \
      typeof(VAL) _val = (VAL); \
      (VM).stack[(VM).sp++] = *(typeof(*(VM).stack) *) &_val; \
    } \
    else { \
      (void) evmIllegalState(&(VM));\
    } \
  } while(0)


#define EVM_POP(VM, COUNT) \
  do { \
    if((VM).sp < ((VM).sp - (typeof((VM).sp)) ((COUNT) + 1))) { \
      (void) evmStackUnderflow(&(VM)); \
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
      (void) evmStackUnderflow(&(VM)); \
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
      (void) evmStackUnderflow(&(VM)); \
    } \
    else { \
      EVM_PUSH(VM, (VM).stack[(VM).sp - (DEPTH)]); \
    } \
  } while(0)


#define EVM_BIN_OP_I(VM, OP) \
  do { \
    if(local.sp < 2U) { (void) evmStackUnderflow(&local); } \
    else { \
      EVM_STACK_I(local, 1) = EVM_TOP_I(local) OP EVM_STACK_I(local, 1); \
      --local.sp; \
    } \
  } while(0)


#define EVM_BIN_OP_F(VM, OP) \
  do { \
    if(local.sp < 2U) { (void) evmStackUnderflow(&local); } \
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

    local.flags &= ~EVM_YIELD; // clear the yield flag if it is set
    EVM_DEBUGF("Running VM for %u operations", maxOps);
    while(ops++ < maxOps && (local.flags & (EVM_HALTED | EVM_YIELD)) == 0) {
      switch(local.program[local.ip]) {
        case OP_NOP:
          EVM_TRACEF("%08X: NOP", local.ip);
          ++local.ip; // do nothing except move to the next instruction
        break;

        case OP_CALL: {
          EVM_TRACEF("%08X: CALL %d", local.ip, evmLoadInt16(&local.program[local.ip + 1]));
          EVM_PUSH(local, local.ip + 3U); // push the return instruction pointer
          // update the instruction pointer to the function
          local.ip += evmLoadInt16(&local.program[local.ip + 1]);
        } break;

        case OP_LCALL:
          EVM_TRACEF("%08X: LCALL %d", local.ip, evmLoadInt24(&local.program[local.ip + 1]));
          EVM_PUSH(local, local.ip + 4U); // push the return instruction pointer
          // update the instruction pointer to the function
          local.ip = evmLoadInt24(&local.program[local.ip + 1]);
        break;

        case OP_BCALL: {
          uint8_t id = local.program[local.ip + 1U];
          EVM_TRACEF("%08X: LCALL %u", local.ip, id);
#if EVM_MAX_BUILTINS != 256
          if(id >= EVM_MAX_BUILTINS) {
            (void) evmIllegalInstruction(&local);
          }
          else {
#endif
          local.ip += 2; // move to the next instruction, allow builtin to override on error
          EVM_PUSH(local, (EVM_BUILTINS[id] ? EVM_BUILTINS[id] : &evmUnboundHandler)(&local)); 
#if EVM_MAX_BUILTINS != 256
          }
#endif
        } break;

        case OP_YIELD:
          EVM_DEBUGF("YIELDING @ %08X", vm->ip);
          ++local.ip; // move to the next instruction
          local.flags |= EVM_YIELD;
        break;

        case OP_HALT:
          EVM_INFOF("Halting @ %08X", vm->ip);
          local.flags |= EVM_HALTED;
        break;

        case OP_PUSH_I0:
          EVM_TRACEF("%08X: PUSH 0", local.ip);
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 0); // push a zero onto the stack
        break;

        case OP_PUSH_I1:
          EVM_TRACEF("%08X: PUSH 1", local.ip);
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 1); // push a one onto the stack
        break;

        case OP_PUSH_IN1:
          EVM_TRACEF("%08X: PUSH -1", local.ip);
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, -1); // push a negative one onto the stack
        break;

        case OP_PUSH_8I:
          EVM_TRACEF("%08X: PUSH %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          local.ip += 2U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt8(&local.program[local.ip - 1U])); // push a signed byte
        break;

        case OP_PUSH_16I:
          EVM_TRACEF("%08X: PUSH %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          local.ip += 3U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt16(&local.program[local.ip - 2U])); // push a signed short
        break;

        case OP_PUSH_24I:
          EVM_TRACEF("%08X: PUSH %d", local.ip, evmLoadInt24(&local.program[local.ip + 1U]));
          local.ip += 4U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt24(&local.program[local.ip - 3U])); // push a signed int24
        break;

        case OP_PUSH_32I:
          EVM_TRACEF("%08X: PUSH %d", local.ip, evmLoadInt32(&local.program[local.ip + 1U]));
          local.ip += 5U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt32(&local.program[local.ip - 4U])); // push a signed int
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_PUSH_F0:
          EVM_TRACEF("%08X: PUSH 0.0", local.ip);
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 0.0f); // push a zero onto the stack
        break;

        case OP_PUSH_F1:
          EVM_TRACEF("%08X: PUSH 1.0", local.ip);
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, 1.0f); // push a one onto the stack
        break;

        case OP_PUSH_FN1:
          EVM_TRACEF("%08X: PUSH -1.0", local.ip);
          ++local.ip; // move to the next instruction
          EVM_PUSH(local, -1.0f); // push a negative one onto the stack
        break;

        case OP_PUSH_F:
          local.ip += 5U; // move to the next instruction
          EVM_PUSH(local, evmLoadInt32(&local.program[local.ip - 4U])); // push a float
          EVM_TRACEF("%08X: PUSH %f", local.ip - 5U, EVM_TOP_F(local));
        break;
#endif

        case OP_SWAP:
          EVM_TRACEF("%08X: SWAP", local.ip);
          ++local.ip; // move to the next instruction
          if(local.sp < 2) { (void) evmStackUnderflow(&local); }
          else {
            uint32_t tmp = local.stack[local.sp - 1U];
            local.stack[local.sp - 1U] = local.stack[local.sp - 2U];
            local.stack[local.sp - 2U] = tmp;
          }
        break;

        case OP_POP_1:
          EVM_TRACEF("%08X: POP 1", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 1U); // remove the top of the stack
        break;

        case OP_POP_2:
          EVM_TRACEF("%08X: POP 2", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 2U); // remove the top two values from the stack
        break;

        case OP_POP_3:
          EVM_TRACEF("%08X: POP 3", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 3U); // remove the top three values from the stack
        break;

        case OP_POP_4:
          EVM_TRACEF("%08X: POP 4", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 4U); // remove the top four values from the stack
        break;

        case OP_POP_5:
          EVM_TRACEF("%08X: POP 5", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 5U); // remove the top five values from the stack
        break;

        case OP_POP_6:
          EVM_TRACEF("%08X: POP 6", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 6U); // remove the top six values from the stack
        break;

        case OP_POP_7:
          EVM_TRACEF("%08X: POP 7", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 7U); // remove the top seven values from the stack
        break;

        case OP_POP_8:
          EVM_TRACEF("%08X: POP 8", local.ip);
          ++local.ip; // move to the next instruction
          EVM_POP(local, 8U); // remove the top eight values from the stack
        break;

        case OP_REM_1:
          EVM_TRACEF("%08X: REM 1", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 1U, 1U); // remove second value from the stack
        break;

        case OP_REM_2:
          EVM_TRACEF("%08X: REM 2", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 2U, 1U); // remove third value from the stack
        break;

        case OP_REM_3:
          EVM_TRACEF("%08X: REM 3", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 3U, 1U); // remove fourth value from the stack
        break;

        case OP_REM_4:
          EVM_TRACEF("%08X: REM 4", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 4U, 1U); // remove fifth value from the stack
        break;

        case OP_REM_5:
          EVM_TRACEF("%08X: REM 5", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 5U, 1U); // remove sixth value from the stack
        break;

        case OP_REM_6:
          EVM_TRACEF("%08X: REM 6", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 6U, 1U); // remove seventh value from the stack
        break;

        case OP_REM_7:
          EVM_TRACEF("%08X: REM 7", local.ip);
          ++local.ip; // move to the next instruction
          EVM_REMOVE(local, 7U, 1U); // remove eighth value from the stack
        break;

        case OP_REM_R:
          EVM_TRACEF("%08X: REM %u %u", local.ip, (local.program[local.ip - 1] >>    4) + 1U,
                                                  (local.program[local.ip - 1] &  0x0F) + 1U);
          local.ip += 2; // move to the next instruction
          // remove up 16 values from a depth of up to 16
          EVM_REMOVE(local, (local.program[local.ip - 1] >>    4) + 1U,
                            (local.program[local.ip - 1] &  0x0F) + 1U);
        break;

        case OP_DUP_0:
          EVM_TRACEF("%08X: DUP 0", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 1U); // duplicate the top stack value
        break;

        case OP_DUP_1:
          EVM_TRACEF("%08X: DUP 1", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 2U); // duplicate the second value in the stack
        break;

        case OP_DUP_2:
          EVM_TRACEF("%08X: DUP 2", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 3U); // duplicate the third value in the stack
        break;

        case OP_DUP_3:
          EVM_TRACEF("%08X: DUP 3", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 4U); // duplicate the fourth value in the stack
        break;

        case OP_DUP_4:
          EVM_TRACEF("%08X: DUP 4", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 5U); // duplicate the fifth value in the stack
        break;

        case OP_DUP_5:
          EVM_TRACEF("%08X: DUP 5", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 6U); // duplicate the sixth value in the stack
        break;

        case OP_DUP_6:
          EVM_TRACEF("%08X: DUP 6", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 7U); // duplicate the seventh value in the stack
        break;

        case OP_DUP_7:
          EVM_TRACEF("%08X: DUP 7", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 8U); // duplicate the eighth value in the stack
        break;

        case OP_DUP_8:
          EVM_TRACEF("%08X: DUP 8", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 9U); // duplicate the nineth value in the stack
        break;

        case OP_DUP_9:
          EVM_TRACEF("%08X: DUP 9", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 10U); // duplicate the tenth value in the stack
        break;

        case OP_DUP_10:
          EVM_TRACEF("%08X: DUP 10", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 11U); // duplicate the eleventh value in the stack
        break;

        case OP_DUP_11:
          EVM_TRACEF("%08X: DUP 11", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 12U); // duplicate the twelfth value in the stack
        break;

        case OP_DUP_12:
          EVM_TRACEF("%08X: DUP 12", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 13U); // duplicate the thirteenth value in the stack
        break;

        case OP_DUP_13:
          EVM_TRACEF("%08X: DUP 13", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 14U); // duplicate the fourteenth value in the stack
        break;

        case OP_DUP_14:
          EVM_TRACEF("%08X: DUP 14", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 15U); // duplicate the fifteenth value in the stack
        break;

        case OP_DUP_15:
          EVM_TRACEF("%08X: DUP 15", local.ip);
          ++local.ip; // move to the next instruction
          EVM_DUP(local, 16U); // duplicate the sixteenth value in the stack
        break;

        case OP_INC_I:
          EVM_TRACEF("%08X: INCI", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { ++EVM_TOP_I(local); } // increment the value on top of the stack
        break;

        case OP_DEC_I:
          EVM_TRACEF("%08X: DECI", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { --EVM_TOP_I(local); } // decrement the value on top of the stack
        break;

        case OP_ABS_I:
          EVM_TRACEF("%08X: ABSI", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_I(local) = abs(EVM_TOP_I(local)); } // absolute value the top of the stack
        break;

        case OP_NEG_I:
          EVM_TRACEF("%08X: NEGI", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_I(local) = -EVM_TOP_I(local); } // negate the top of the stack
        break;

        case OP_ADD_I:
          EVM_TRACEF("%08X: ADDI", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, +); // replace the top two values with their sum
        break;

        case OP_SUB_I:
          EVM_TRACEF("%08X: SUB", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, -); // replace the top two values with their difference
        break;

        case OP_MUL_I:
          EVM_TRACEF("%08X: MULI", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, *); // replace the top two values with their product
        break;

        case OP_DIV_I:
          EVM_TRACEF("%08X: DIVI", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, /); // replace the top two values with their quotient
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_INC_F:
          EVM_TRACEF("%08X: INCF", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_F(local) += 1.0f; } // increment the value on top of the stack
        break;

        case OP_DEC_F:
          EVM_TRACEF("%08X: DECF", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_F(local) -= 1.0f; } // decrement the value on top of the stack
        break;

        case OP_ABS_F:
          EVM_TRACEF("%08X: ABSF", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_F(local) = fabs(EVM_TOP_F(local)); } // absolute value the stack top
        break;

        case OP_NEG_F:
          EVM_TRACEF("%08X: NEGF", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_F(local) = -EVM_TOP_F(local); } // negate the top of the stack
        break;

        case OP_ADD_F:
          EVM_TRACEF("%08X: ADDF", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, +); // replace the top two values with their sum
        break;

        case OP_SUB_F:
          EVM_TRACEF("%08X: SUBF", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, -); // replace the top two values with their difference
        break;

        case OP_MUL_F:
          EVM_TRACEF("%08X: MULF", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, *); // replace the top two values with their product
        break;

        case OP_DIV_F:
          EVM_TRACEF("%08X: DIVF", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_F(local, /); // replace the top two values with their quotient
        break;
#endif

        case OP_LSH:
          EVM_TRACEF("%08X: LSH", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, <<);
        break;

        case OP_RSH:
          EVM_TRACEF("%08X: RSH", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, >>);
        break;

        case OP_AND:
          EVM_TRACEF("%08X: AND", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, &);
        break;

        case OP_OR:
          EVM_TRACEF("%08X: OR", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, |);
        break;

        case OP_XOR:
          EVM_TRACEF("%08X: XOR", local.ip);
          ++local.ip; // move to the next instruction
          EVM_BIN_OP_I(local, ^);
        break;

        case OP_INV:
          EVM_TRACEF("%08X: INV", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_I(local) = ~EVM_TOP_I(local); }
        break;

        case OP_BOOL:
          EVM_TRACEF("%08X: BOOL", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_I(local) = !!EVM_TOP_I(local); }
        break;

        case OP_NOT:
          EVM_TRACEF("%08X: NOT", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_I(local) = !EVM_TOP_I(local); }
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_CONV_FI:
          EVM_TRACEF("%08X: CONVFI 0", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_I(local) = (int32_t) EVM_TOP_F(local); }
        break;

        case OP_CONV_FI_1:
          EVM_TRACEF("%08X: CONVFI 1", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_STACK_I(local, 1U) = (int32_t) EVM_STACK_F(local, 1U); }
        break;

        case OP_CONV_IF:
          EVM_TRACEF("%08X: CONVIF 0", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_TOP_F(local) = (float) EVM_TOP_I(local); }
        break;

        case OP_CONV_IF_1:
          EVM_TRACEF("%08X: CONVIF 1", local.ip);
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else { EVM_STACK_F(local, 1U) = (float) EVM_STACK_I(local, 1U); }
        break;
#endif

        case OP_CMP_I0:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else {
            int32_t val = EVM_TOP_I(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 0) {       local.flags |= EVM_LESS;    }
            else if(val == 0) { local.flags |= EVM_EQUAL;   }
            else {              local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %d <=> 0", local.ip - 1U, val);
          }
        break;

        case OP_CMP_I1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else {
            int32_t val = EVM_TOP_I(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 1) {       local.flags |= EVM_LESS;    }
            else if(val == 1) { local.flags |= EVM_EQUAL;   }
            else {              local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %d <=> 1", local.ip - 1U, val);
          }
        break;

        case OP_CMP_IN1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else {
            int32_t val = EVM_TOP_I(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < -1) {       local.flags |= EVM_LESS;    }
            else if(val == -1) { local.flags |= EVM_EQUAL;   }
            else {               local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %d <=> -1", local.ip - 1U, val);
          }
        break;

        case OP_CMP_I:
          ++local.ip; // move to the next instruction
          if(local.sp < 2U) { (void) evmStackUnderflow(&local); }
          else {
            int32_t lhs = EVM_TOP_I(local);
            int32_t rhs = EVM_STACK_I(local, 1U);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(lhs < rhs) {       local.flags |= EVM_LESS;    }
            else if(lhs == rhs) { local.flags |= EVM_EQUAL;   }
            else {                local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %d <=> %d", local.ip - 1U, lhs, rhs);
          }
        break;

#if EVM_FLOAT_SUPPORT == 1
        case OP_CMP_F0:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else {
            float val = EVM_TOP_F(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 0.0f) {       local.flags |= EVM_LESS;    }
            else if(val == 0.0f) { local.flags |= EVM_EQUAL;   }
            else {                 local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %f <=> 0.0", local.ip - 1U, val);
          }
        break;

        case OP_CMP_F1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else {
            float val = EVM_TOP_F(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < 1.0f) {       local.flags |= EVM_LESS;    }
            else if(val == 1.0f) { local.flags |= EVM_EQUAL;   }
            else {                 local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %f <=> 1.0", local.ip - 1U, val);
          }
        break;

        case OP_CMP_FN1:
          ++local.ip; // move to the next instruction
          if(!local.sp) { (void) evmStackUnderflow(&local); }
          else {
            float val = EVM_TOP_F(local);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(val < -1.0f) {       local.flags |= EVM_LESS;    }
            else if(val == -1.0f) { local.flags |= EVM_EQUAL;   }
            else {                  local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %f <=> -1.0", local.ip - 1U, val);
          }
        break;

        case OP_CMP_F:
          ++local.ip; // move to the next instruction
          if(local.sp < 2U) { (void) evmStackUnderflow(&local); }
          else {
            float lhs = EVM_TOP_F(local);
            float rhs = EVM_STACK_F(local, 1U);
            local.flags &= ~(EVM_LESS | EVM_EQUAL | EVM_GREATER);
            if(lhs < rhs) {       local.flags |= EVM_LESS;    }
            else if(lhs == rhs) { local.flags |= EVM_EQUAL;   }
            else {                local.flags |= EVM_GREATER; }
            EVM_TRACEF("%08X: CMP %f <=> %f", local.ip - 1U, lhs, rhs);
          }
        break;
#endif

        case OP_JMP:
          EVM_TRACEF("%08X: JMP %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
        break;

        case OP_JLT:
          EVM_TRACEF("%08X: JLT %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          if(local.flags & EVM_LESS) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JLE:
          EVM_TRACEF("%08X: JLE %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          if(local.flags & (EVM_LESS | EVM_EQUAL)) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JNE:
          EVM_TRACEF("%08X: JNE %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          if(local.flags & (EVM_LESS | EVM_GREATER)) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JEQ:
          EVM_TRACEF("%08X: JEQ %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          if(local.flags & EVM_EQUAL) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JGE:
          EVM_TRACEF("%08X: JGE %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          if(local.flags & (EVM_GREATER | EVM_EQUAL)) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JGT:
          EVM_TRACEF("%08X: JGT %d", local.ip, evmLoadInt8(&local.program[local.ip + 1U]));
          if(local.flags & EVM_GREATER) {
            local.ip += evmLoadInt8(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJMP:
          EVM_TRACEF("%08X: LJMP %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
        break;

        case OP_LJLT:
          EVM_TRACEF("%08X: LJLT %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          if(local.flags & EVM_LESS) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJLE:
          EVM_TRACEF("%08X: LJLE %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          if(local.flags & (EVM_LESS | EVM_EQUAL)) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJNE:
          EVM_TRACEF("%08X: LJNE %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          if(local.flags & (EVM_LESS | EVM_GREATER)) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJEQ:
          EVM_TRACEF("%08X: LJEQ %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          if(local.flags & EVM_EQUAL) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJGE:
          EVM_TRACEF("%08X: LJGE %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          if(local.flags & (EVM_GREATER | EVM_EQUAL)) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_LJGT:
          EVM_TRACEF("%08X: LJGT %d", local.ip, evmLoadInt16(&local.program[local.ip + 1U]));
          if(local.flags & EVM_GREATER) {
            local.ip += evmLoadInt16(&local.program[local.ip + 1U]);
          }
          else {
            local.ip += 2;
          }
        break;

        case OP_JTBL:
          if(!local.sp) {
            (void) evmStackUnderflow(&local);
          }
          else {
            EVM_TRACEF("%08X: JTBL %d => %d",
                local.ip, EVM_TOP_I(local),
                evmLoadInt8(&local.program[EVM_TOP_I(local) + local.ip + 1U]));
            local.ip += evmLoadInt8(&local.program[EVM_TOP_I(local) + local.ip + 1U]);
          }
        break;

        case OP_LJTBL:
          if(!local.sp) {
            (void) evmStackUnderflow(&local);
          }
          else {
            EVM_TRACEF("%08X: LJTBL %d => %d",
                local.ip, EVM_TOP_I(local),
                evmLoadInt16(&local.program[EVM_TOP_I(local) * 2 + local.ip + 1U]));
            local.ip += evmLoadInt16(&local.program[EVM_TOP_I(local) * 2 + local.ip + 1U]);
          }
        break;

        case OP_RET:
          EVM_TRACEF("%08X: RET 0", local.ip);
          if(!local.sp) {
            (void) evmStackUnderflow(&local);
          }
          else {
            local.ip = (uint32_t) EVM_TOP_I(local);
            --local.sp;
          }
        break;

        case OP_RET_1:
          EVM_TRACEF("%08X: RET 1", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 1U);
          EVM_REMOVE(local, 1U, 1U); // remove return address from the stack
        break;

        case OP_RET_2:
          EVM_TRACEF("%08X: RET 2", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 2U);
          EVM_REMOVE(local, 2U, 1U); // remove return address from the stack
        break;

        case OP_RET_3:
          EVM_TRACEF("%08X: RET 3", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 3U);
          EVM_REMOVE(local, 3U, 1U); // remove return address from the stack
        break;

        case OP_RET_4:
          EVM_TRACEF("%08X: RET 4", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 4U);
          EVM_REMOVE(local, 4U, 1U); // remove return address from the stack
        break;

        case OP_RET_5:
          EVM_TRACEF("%08X: RET 5", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 5U);
          EVM_REMOVE(local, 5U, 1U); // remove return address from the stack
        break;

        case OP_RET_6:
          EVM_TRACEF("%08X: RET 6", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 6U);
          EVM_REMOVE(local, 6U, 1U); // remove return address from the stack
        break;

        case OP_RET_7:
          EVM_TRACEF("%08X: RET 7", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 7U);
          EVM_REMOVE(local, 7U, 1U); // remove return address from the stack
        break;

        case OP_RET_8:
          EVM_TRACEF("%08X: RET 8", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 8U);
          EVM_REMOVE(local, 8U, 1U); // remove return address from the stack
        break;

        case OP_RET_9:
          EVM_TRACEF("%08X: RET 9", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 9U);
          EVM_REMOVE(local, 9U, 1U); // remove return address from the stack
        break;

        case OP_RET_10:
          EVM_TRACEF("%08X: RET 10", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 10U);
          EVM_REMOVE(local, 10U, 1U); // remove return address from the stack
        break;

        case OP_RET_11:
          EVM_TRACEF("%08X: RET 11", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 11U);
          EVM_REMOVE(local, 11U, 1U); // remove return address from the stack
        break;

        case OP_RET_12:
          EVM_TRACEF("%08X: RET 12", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 12U);
          EVM_REMOVE(local, 12U, 1U); // remove return address from the stack
        break;

        case OP_RET_13:
          EVM_TRACEF("%08X: RET 13", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 13U);
          EVM_REMOVE(local, 13U, 1U); // remove return address from the stack
        break;

        case OP_RET_14:
          EVM_TRACEF("%08X: RET 14", local.ip);
          local.ip = (uint32_t) EVM_STACK_I(local, 14U);
          EVM_REMOVE(local, 14U, 1U); // remove return address from the stack
        break;

        case OP_RET_I: {
          uint32_t depth = local.program[local.ip + 1U];
          EVM_TRACEF("%08X: RET %u", local.ip, depth);
          local.ip = (uint32_t) EVM_STACK_I(local, depth);
          EVM_REMOVE(local, depth, 1U); // remove return address from the stack
        } break;

        default:
          EVM_TRACEF("%08X: ILLEGAL(%02X)", local.ip, local.program[local.ip]);
          // invoke illegal instruction handler
          (void) evmIllegalInstruction(&local);
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

