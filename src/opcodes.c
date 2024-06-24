#include "evm/opcodes.h"


#ifdef __cplusplus
extern "C" {
#endif


#define INVAL "!INVALID!"


static const char *MNEMONIC_STRINGS[256] = {
  // FAM_CALL
  "NOP",  "CALL", "CALL", "BLTIN", INVAL, INVAL,  INVAL,   INVAL,
   INVAL,  INVAL,  INVAL,  INVAL,  INVAL, INVAL, "YIELD", "HALT",
  // FAM_PUSH
  "PUSH",  "PUSH",  "PUSH",  "PUSH",  "PUSH", "PUSH", "PUSH",
#if EVM_FLOAT_SUPPORT == 1
  "PUSHF", "PUSHF", "PUSHF", "PUSHF",
#else
   INVAL,   INVAL,   INVAL,   INVAL,
#endif
   INVAL,   INVAL,   INVAL,   INVAL,  "SWAP",
  // FAM_POP
  "POP",  "POP",  "POP",  "POP", "POP", "POP", "POP", "POP", 
  "POP",  "POP",  "POP",  "POP", "POP", "POP", "POP", "POP", 
  // FAM_DUP
  "DUP",  "DUP",  "DUP",  "DUP", "DUP", "DUP", "DUP", "DUP", 
  "DUP",  "DUP",  "DUP",  "DUP", "DUP", "DUP", "DUP", "DUP", 
  // FAM_MATH
  "INC",  "DEC",  "ABS",  "NEG",  "ADD",  "SUB",  "MUL",  "DIV",
#if EVM_FLOAT_SUPPORT == 1
  "INCF", "DECF", "ABSF", "NEGF", "ADDF", "SUBF", "MULF", "DIVF",
#else
   INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
#endif
   // FAM_BITS
   "LSH",   "RSH",   "AND",   "OR",    "XOR", "INV", "BOOL", "NOT"
#if EVM_FLOAT_SUPPORT == 1
   "CNVFI", "CNVFI", "CNVIF", "CNVIF",
#else
    INVAL,   INVAL,   INVAL,   INVAL,
#endif
    INVAL,   INVAL,   INVAL,   INVAL,
   // 0x60
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // 0x70
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // 0x80
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // 0x90
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // 0xA0
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // 0xB0
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // 0xC0
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // FAM_CMP
   "CMP",  "CMP",  "CMP",  "CMP"
#if EVM_FLOAT_SUPPORT == 1
   "CMPF", "CMPF", "CMPF", "CMPF", 
#else
    INVAL,  INVAL,  INVAL,  INVAL,
#endif
    INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,  INVAL,
   // FAM_JMP
   "JMP", "JMP", "JMP", "JMP", "JMP", "JMP", "JMP", "JMP",
   "JMP", "JMP", "JMP", "JMP", "JMP", "JMP", "JMP", "JMP",
   // FAM_RET
   "RET", "RET", "RET", "RET", "RET", "RET", "RET", "RET",
   "RET", "RET", "RET", "RET", "RET", "RET", "RET", "RET"
};


const char *evmOpcodeToMnemonic(opcode_t op) {
  return MNEMONIC_STRINGS[op & 0xFF];
}


#ifdef __cplusplus
}
#endif

