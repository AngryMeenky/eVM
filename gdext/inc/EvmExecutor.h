#ifndef GDEXT_EVM_EXECUTOR_H
#  define GDEXT_EVM_EXECUTOR_H


#include "extcfg.h"
#include "evm.h"

#include <array>
#include <memory>


#ifdef GDEXTENSION
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#endif

namespace godot {

class EvmExecutor: public virtual RefCounted {
  GDCLASS(EvmExecutor, RefCounted)

public:
  EvmExecutor();
  virtual ~EvmExecutor();

  bool canRun() const;
  int  run(int);

  bool hasYielded() const;
  bool hasHalted() const;

  bool setProgram(PackedByteArray, int);
  int  programSize() const;
  int  instructionPointer() const;

  bool    setStackSize(int);
  int     maximumStack() const;
  int     stackDepth() const;
  int32_t topInt() const;
  int32_t stackInt(int) const;
  bool    pushInt(int32_t);
#if EVM_FLOAT_SUPPORT == 1
  float   topFloat() const;
  float   stackFloat(int) const;
  bool    pushFloat(float);
#endif
  bool    pop();

  void     setBuiltin(int, Callable);
  Callable getBuiltin(int) const;

  void    setEnvironment(Variant);
  Variant getEnvironment() const;

  int32_t callBuiltin(int idx);

protected:
  static void _bind_methods();


private:
  Variant                                 env;
  Array                                   args;
  evm_t                                  *evm;
  std::unique_ptr<int32_t[]>              stack;
  std::array<Callable, EVM_MAX_BUILTINS>  builtins;
};


}


#endif /* GDEXT_EVM_EXECUTOR_H */

