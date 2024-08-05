#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/builtin_types.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "EvmExecutor.h"

using namespace godot;


EvmExecutor::EvmExecutor():
  env(),
  args(),
#if EVM_STATIC_STACK == 1
  evm(evmInitialize(evmAllocate(), this, nullptr, 0)),
#else
  evm(evmInitialize(evmAllocate(), this, 0)),
#endif
  stack(nullptr),
  builtins{} {
}


EvmExecutor::~EvmExecutor() {
  evmFree(evmFinalize(evm));
  evm = nullptr;
}


bool EvmExecutor::canRun() const {
  return evmProgramSize(evm) > 0 && evmStackDepth(evm) > 0;
}


int EvmExecutor::run(int maxOps) {
  if(canRun()) {
    return evmRun(evm, maxOps);
  }

  return -1;
}


bool EvmExecutor::hasYielded() const {
  return evmHasYielded(evm) == 1; // only return true on actual yield
}


bool EvmExecutor::hasHalted() const {
  return evmHasHalted(evm) != 0; // return true on failure or halt
}


bool EvmExecutor::setProgram(PackedByteArray arr, int length) {
  if(length <= 0 || length > arr.size()) {
    length = static_cast<int32_t>(arr.size());
  }

  return !evmSetProgram(evm, arr.ptr(), length);
}


int EvmExecutor::programSize() const {
  return evmProgramSize(evm);
}


int EvmExecutor::instructionPointer() const {
  return evmInstructionIndex(evm);
}


bool EvmExecutor::setStackSize(int size) {
  if(size <= 0xFFFF && size >= evmStackDepth(evm)) {
    if(size != evmMaximumStack(evm)) {
      // copy the old stack to the new stack
      std::unique_ptr<int32_t[]> newStack(new int32_t[size]);
      memcpy(newStack.get(), stack.get(), evmStackDepth(evm) * sizeof(int32_t));

      std::swap(stack, newStack);
      evm->stack = stack.get();
      evmMaximumStack(evm) = static_cast<uint16_t>(size);
    }

    return true;
  }

  return false;
}


int EvmExecutor::maximumStack() const {
  return evmMaximumStack(evm);
}


int EvmExecutor::stackDepth() const {
  return evmStackDepth(evm);
}


int32_t EvmExecutor::topInt() const {
  return evmStackTop(evm);
}


int32_t EvmExecutor::stackInt(int depth) const {
  return evmStackValue(evm, depth);
}


bool EvmExecutor::pushInt(int32_t val) {
  return !evmPush(evm, val);
}


#if EVM_FLOAT_SUPPORT == 1
float EvmExecutor::topFloat() const {
  return evmStackTopf(evm);
}


float EvmExecutor::stackFloat(int depth) const {
  return evmStackValuef(evm, depth);
}


bool EvmExecutor::pushFloat(float val) {
  return !evmPushf(evm, val);
}
#endif


bool EvmExecutor::pop() {
  return !evmPop(evm);
}


void EvmExecutor::setBuiltin(int idx, Callable var) {
  if(idx <= 0 && idx < (int) builtins.size()) {
    builtins[idx] = Variant(var);
  }
}


Callable EvmExecutor::getBuiltin(int idx) const {
  if(idx < 0 || idx >= (int) builtins.size()) {
    return Callable();
  }

  return builtins[idx];
}


void EvmExecutor::setEnvironment(Variant v) {
  env = v;
}


Variant EvmExecutor::getEnvironment() const {
  return env;
}


int32_t EvmExecutor::callBuiltin(int idx) {
  if(Callable builtin = getBuiltin(idx); builtin.is_valid()) {
    args.push_back(this);
    int32_t result = static_cast<int32_t>(builtin.callv(args));
    args.pop_back();
    return result;
  }
  else {
    return evmUnboundHandler(evm);
  }
}


void EvmExecutor::_bind_methods() {
  ClassDB::bind_method(D_METHOD("can_run"), &EvmExecutor::canRun);
  ClassDB::bind_method(D_METHOD("run", "ops"), &EvmExecutor::run);

  ClassDB::bind_method(D_METHOD("has_yielded"), &EvmExecutor::hasYielded);
  ClassDB::bind_method(D_METHOD("has_halted"), &EvmExecutor::hasHalted);

  ClassDB::bind_method(D_METHOD("set_program", "length"), &EvmExecutor::setProgram, DEFVAL(-1));
  ClassDB::bind_method(D_METHOD("program_size"), &EvmExecutor::programSize);
  ClassDB::bind_method(D_METHOD("instruction_pointer"), &EvmExecutor::instructionPointer);

  ClassDB::bind_method(D_METHOD("set_stack_size", "depth"), &EvmExecutor::setStackSize);
  ClassDB::bind_method(D_METHOD("get_stack_size"), &EvmExecutor::maximumStack);
  ClassDB::bind_method(D_METHOD("get_stack_depth"), &EvmExecutor::stackDepth);
  ClassDB::bind_method(D_METHOD("stack_top_int"), &EvmExecutor::topInt);
  ClassDB::bind_method(D_METHOD("stack_int", "depth"), &EvmExecutor::stackInt);
  ClassDB::bind_method(D_METHOD("push_int", "value"), &EvmExecutor::pushInt);
#if EVM_FLOAT_SUPPORT == 1
  ClassDB::bind_method(D_METHOD("stack_top_float"), &EvmExecutor::topFloat);
  ClassDB::bind_method(D_METHOD("stack_float", "depth"), &EvmExecutor::stackFloat);
  ClassDB::bind_method(D_METHOD("push_float", "value"), &EvmExecutor::pushFloat);
#endif
  ClassDB::bind_method(D_METHOD("pop"), &EvmExecutor::pop);

  ClassDB::bind_method(D_METHOD("set_builtin", "index", "function"), &EvmExecutor::setBuiltin);
  ClassDB::bind_method(D_METHOD("get_builtin", "index"), &EvmExecutor::getBuiltin);

  ClassDB::bind_method(D_METHOD("set_environment", "value"), &EvmExecutor::setEnvironment);
  ClassDB::bind_method(D_METHOD("get_environment"), &EvmExecutor::getEnvironment);
}


static int32_t evmAdapter(evm_t *evm) {
  int result = -1;
  EVM_TRACEF("Enter %s", __FUNCTION__);

  if(evm) {
    if(evm->env) {
      EvmExecutor *vm = reinterpret_cast<EvmExecutor *>(evm->env);
      EVM_TRACEF("Calling builtin #%d", evm->program[evm->ip - 1U]);
      result = vm->callBuiltin(evm->program[evm->ip - 1U]);
    }
    else {
      EVM_ERRORF("Called builtin with missing godot wrapper @ %08X", evm->ip - 2U);
    }
  }
  else {
    EVM_ERROR("Called builtin with NULL eVM");
  }

  EVM_TRACEF("Exit %s", __FUNCTION__);
  return result;
}


const EvmBuiltinFunction EVM_BUILTINS[EVM_MAX_BUILTINS] = {
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter,
  &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter, &evmAdapter
};

