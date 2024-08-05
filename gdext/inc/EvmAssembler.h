#ifndef GDEXT_EVM_ASSEMBLER_H
#  define GDEXT_EVM_ASSEMBLER_H


#include "extcfg.h"
#include "evm/asm.h"

#include <array>


#ifdef GDEXTENSION
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#endif

namespace godot {

class EvmAssembler: public virtual RefCounted {
  GDCLASS(EvmAssembler, RefCounted)


public:
  EvmAssembler();
  virtual ~EvmAssembler();

  bool parseFile(const String &, FileAccess *);
  bool parseLine(const String &, const String &, int);

  int validateProgram();

  PackedByteArray toBuffer();
  bool            toFile(FileAccess *);


protected:
  static void _bind_methods();


private:
  evm_assembler_t *evm;
};


}


#endif /* GDEXT_EVM_ASSEMBLER_H */

