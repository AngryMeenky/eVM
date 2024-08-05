#include "EvmDisassembler.h"


using namespace godot;


EvmDisassembler::EvmDisassembler():
  evm(evmdisInitialize(evmdisAllocate())) {
}


EvmDisassembler::~EvmDisassembler() {
  evmdisFree(evmdisFinalize(evm));
  evm = nullptr;
}


bool EvmDisassembler::fromFile(FileAccess *fa) {
  if(fa && fa->is_open()) {
    PackedByteArray arr = fa->get_buffer(fa->get_length() - fa->get_position());

    if(fa->get_error() == OK) {
      return fromBuffer(arr);
    }
  }

  return false;
}


bool EvmDisassembler::fromBuffer(const PackedByteArray &arr) {
  return false;
}


PackedStringArray EvmDisassembler::toBuffer() {
  return PackedStringArray();
}


bool EvmDisassembler::toFile(FileAccess *fa) {
  PackedByteArray arr = toBuffer().to_byte_array();

  if(arr.size() > 0) {
    fa->store_buffer(arr);

    return fa->get_error() == OK;
  }

  return false;
}


void EvmDisassembler::_bind_methods() {
  ClassDB::bind_method(D_METHOD("from_buffer", "src"), &EvmDisassembler::fromBuffer);
  ClassDB::bind_method(D_METHOD("from_file", "src"), &EvmDisassembler::fromFile);

  ClassDB::bind_method(D_METHOD("to_buffer"), &EvmDisassembler::toBuffer);
  ClassDB::bind_method(D_METHOD("to_file", "dst"), &EvmDisassembler::toFile);
}

