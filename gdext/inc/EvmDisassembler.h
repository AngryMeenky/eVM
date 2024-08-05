#ifndef GDEXT_EVM_DISASSEMBLER_H
#  define GDEXT_EVM_DISASSEMBLER_H


#include "extcfg.h"
#include "evm/disasm.h"

#include <array>


#ifdef GDEXTENSION
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#endif

namespace godot {

class EvmDisassembler: public virtual RefCounted {
  GDCLASS(EvmDisassembler, RefCounted)

public:
  EvmDisassembler();
  virtual ~EvmDisassembler();

  bool fromFile(FileAccess *);
  bool fromBuffer(const PackedByteArray &);

  PackedByteArray toBuffer();
  bool            toFile(FileAccess *);


protected:
  static void _bind_methods();


private:
  evm_disassembler_t *evm;
};


}


#endif /* GDEXT_EVM_DISASSEMBLER_H */

