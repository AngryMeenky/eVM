#include "EvmAssembler.h"


using namespace godot;


EvmAssembler::EvmAssembler():
  evm(evmasmInitialize(evmasmAllocate())) {
}


EvmAssembler::~EvmAssembler() {
  evmasmFree(evmasmFinalize(evm));
  evm = nullptr;
}


bool EvmAssembler::parseFile(const String &name, FileAccess *fa) {
  bool result;

  if((result = (fa && fa->is_open()))) {
    int num = 0;

    while(!fa->eof_reached()) {
      result &= parseLine(name, fa->get_line(), ++num);
    }
  }

  return result;
}


bool EvmAssembler::parseLine(const String &name, const String &str, int num) {
  return !evmasmParseLine(evm, name.utf8().get_data(), str.utf8().get_data(), num);
}


int EvmAssembler::validateProgram() {
  return evmasmValidateProgram(evm);
}


PackedByteArray EvmAssembler::toBuffer() {
  PackedByteArray result;
  int64_t length = evmasmProgramSize(evm);

  if(length > 0) {
    result.resize(length); // make sure the buffer is large enough
    length = evmasmProgramToBuffer(evm, result.ptrw(), static_cast<uint32_t>(length));
    result.resize(length); // trim to length
  }

  return result;
}


bool EvmAssembler::toFile(FileAccess *fa) {
  if(fa && fa->is_open()) {
    PackedByteArray bits = toBuffer();

    if(bits.size() > 0) {
      fa->store_buffer(bits);
      return fa->get_error() == OK;
    }
  }

  return false;
}


void EvmAssembler::_bind_methods() {
  ClassDB::bind_method(D_METHOD("parse_file", "name", "file"), &EvmAssembler::parseFile);
  ClassDB::bind_method(D_METHOD("parse_line", "name", "line", "num"), &EvmAssembler::parseLine);

  ClassDB::bind_method(D_METHOD("validate_program"), &EvmAssembler::validateProgram);
  ClassDB::bind_method(D_METHOD("to_buffer"), &EvmAssembler::toBuffer);
  ClassDB::bind_method(D_METHOD("to_file", "dst"), &EvmAssembler::toFile);
}

