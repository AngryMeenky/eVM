#ifndef EVM_EVM_H
#  define EVM_EVM_H


#include "evm/config.h"

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// the state of the virtual machine
typedef struct evm_s {
  uint32_t       ip;
  uint16_t       sp;
  uint16_t       maxStack;
  uint32_t       maxProgram;
  uint32_t       flags;
  int32_t       *stack;
  const uint8_t *program;
  void          *env;
} evm_t;


typedef enum evm_flags_e {
  EVM_LESS    = 1 <<  0,
  EVM_EQUAL   = 1 <<  1,
  EVM_GREATER = 1 <<  2,

  EVM_YIELD   = 1 << 30,
  EVM_HALTED  = 1 << 31,
} evm_flags_t;


// a list of builtin functions callable from the virtual machine
typedef int32_t (*EvmBuiltinFunction)(evm_t *);
extern const EvmBuiltinFunction EVM_BUILTINS[EVM_MAX_BUILTINS];


EVM_API int32_t evmUnboundHandler(evm_t *vm);


// eVM lifecycle functions
EVM_API evm_t *evmAllocate();
#if EVM_STATIC_STACK == 1
EVM_API evm_t *evmInitialize(evm_t *vm, void *user, int32_t *stack, uint16_t stackSize);
#else
EVM_API evm_t *evmInitialize(evm_t *vm, void *user, uint16_t stackSize);
#endif
EVM_API evm_t *evmFinalize(evm_t *vm);
EVM_API void   evmFree(evm_t *vm);


// set the program for the virtual machine instance
EVM_API int evmSetProgram(evm_t *vm, const uint8_t *prog, uint32_t length);

// execute the virtual machine for the given number of operations
EVM_API int evmRun(evm_t *vm, uint32_t maxOps);

// status functions
EVM_API int evmHasHalted(const evm_t *);
EVM_API int evmHasYielded(const evm_t *);

// simple query "functions"
#define evmMaximumStack(EVM_PTR)    ((EVM_PTR)->maxStack)
#define evmStackDepth(EVM_PTR)      ((EVM_PTR)->sp)
#define evmStackValue(EVM_PTR, IDX) ((EVM_PTR)->stack[(EVM_PTR)->sp - ((IDX) - 1U)])
#define evmStackTop(EVM_PTR)        evmStackValue(EVM_PTR, 0U)
#if EVM_FLOAT_SUPPORT == 1
#  define evmStackTopf(EVM_PTR)     evmStackValuef(EVM_PTR, 0U)
#  define evmStackValuef(EVM_PTR, IDX) (*(float *) &(EVM_PTR)->stack[(EVM_PTR)->sp - ((IDX) - 1U)])
#endif

#define evmProgramSize(EVM_PTR) ((EVM_PTR)->maxProgram)
#define evmInstructionIndex(EVM_PTR) ((EVM_PTR)->ip)


EVM_API int evmPush(evm_t *, int32_t);
#if EVM_FLOAT_SUPPORT == 1
EVM_API int evmPushf(evm_t *, float);
#endif
EVM_API int evmPop(evm_t *);


#ifdef __cplusplus
}
#endif


#endif

