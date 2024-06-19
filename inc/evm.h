#ifndef EVM_EVM_H
#  define EVM_EVM_H


#include "evm/evm_config.h"

#include <stdint.h>


// the state of the virtual machine
typedef struct evm_s {
  uint32_t  ip;
  uint16_t  sp;
  uint16_t  maxStack;
  uint32_t  maxProgram;
  uint32_t  flags;
  int32_t  *stack;
  uint8_t  *program;
  void     *env;
} evm_t;


typedef enum evm_flags_e {
  EVM_LESS    = 1 <<  0,
  EVM_EQUAL   = 1 <<  1,
  EVM_GREATER = 1 <<  2,

  EVM_HALTED  = 1 << 31,
} evm_flags_t;


// a list of builtin functions callable from the virtual machine
typedef int32_t (*EvmBuiltinFunction)(evm_t *);
extern const EvmBuiltinFunction EVM_BUILTINS[EVM_MAX_BUILTINS];

#define EVM_ILLEGAL_INSTRUCTION (EVM_MAX_BUILTINS - 1)
#define EVM_ILLEGAL_STATE       (EVM_MAX_BUILTINS - 2)
#define EVM_STACK_OVERFLOW      (EVM_MAX_BUILTINS - 3)
#define EVM_STACK_UNDERFLOW     (EVM_MAX_BUILTINS - 4)

int32_t evmUnboundHandler(evm_t *vm);
int32_t evmIllegalStateHandler(evm_t *vm);
int32_t evmStackOverflowHandler(evm_t *vm);
int32_t evmStackUnderflowHandler(evm_t *vm);
int32_t evmIllegalInstructionHandler(evm_t *vm);

// eVM lifecycle functions
evm_t *evmAllocate();
evm_t *evmInitialize(evm_t *vm, void *user, uint16_t stackSize);
void  *evmFinalize(evm_t *vm);
void   evmFree(evm_t *vm);


// set the program for the virtual machine instance
int evmSetProgram(evm_t *vm, uint8_t *prog, uint32_t length);

// execute the virtual machine for the given number of operations
int evmRun(evm_t *vm, uint32_t maxOps);

// status functions
int evmHasHalted(const evm_t *);


#endif

