#ifndef EVM_OPCODES_H
#  define EVM_OPCODES_H

#include "evm/config.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum opcode_family_e {
  FAM_CALL = 0x00,
  FAM_PUSH = 0x10,
  FAM_POP  = 0x20,
  FAM_DUP  = 0x30,
  FAM_MATH = 0x40,
  FAM_BITS = 0x50,

  // 0x60-0xC0 reserved for future expansion

  FAM_CMP  = 0xD0,
  FAM_JMP  = 0xE0,
  FAM_RET  = 0xF0,

} opcode_family_t;


typedef enum opcode_e {
  OP_NOP   = FAM_CALL | 0x00, // do nothing
  OP_CALL  = FAM_CALL | 0x01, // near call next two bytes as offset
  OP_LCALL = FAM_CALL | 0x02, // far call next 3 bytes as offset
  OP_BCALL = FAM_CALL | 0x03, // call a builtin by using the next byte as an ID
  OP_YIELD = FAM_CALL | 0x0E, // suspend execution
  OP_HALT  = FAM_CALL | 0x0F, // end execution

  OP_PUSH_I0  = FAM_PUSH | 0x00, // push 0
  OP_PUSH_I1  = FAM_PUSH | 0x01, // push 1
  OP_PUSH_IN1 = FAM_PUSH | 0x02, // push -1
  OP_PUSH_8I  = FAM_PUSH | 0x03, // push the next byte as an integer
  OP_PUSH_16I = FAM_PUSH | 0x04, // push the next 2 bytes as a little endian integer
  OP_PUSH_24I = FAM_PUSH | 0x05, // push the next 3 bytes as a little endian integer
  OP_PUSH_32I = FAM_PUSH | 0x06, // push the next 4 bytes as a little endian integer
#if EVM_FLOAT_SUPPORT == 1
  OP_PUSH_F0  = FAM_PUSH | 0x07, // push 0.0
  OP_PUSH_F1  = FAM_PUSH | 0x08, // push 1.0
  OP_PUSH_FN1 = FAM_PUSH | 0x09, // push -1.0
  OP_PUSH_F   = FAM_PUSH | 0x0A, // push the next 4 bytes as a little endian float
#endif
  OP_SWAP     = FAM_PUSH | 0x0F, // swap the top two values on the stack

  OP_POP_1 = FAM_POP | 0x00, // pop 1 value off the stack
  OP_POP_2 = FAM_POP | 0x01, // pop 2 values off the stack
  OP_POP_3 = FAM_POP | 0x02, // pop 3 values off the stack
  OP_POP_4 = FAM_POP | 0x03, // pop 4 values off the stack
  OP_POP_5 = FAM_POP | 0x04, // pop 5 values off the stack
  OP_POP_6 = FAM_POP | 0x05, // pop 6 values off the stack
  OP_POP_7 = FAM_POP | 0x06, // pop 7 values off the stack
  OP_POP_8 = FAM_POP | 0x07, // pop 8 values off the stack
  OP_REM_1 = FAM_POP | 0x08, // remove the second value from the stack
  OP_REM_2 = FAM_POP | 0x09, // remove the third value from the stack
  OP_REM_3 = FAM_POP | 0x0A, // remove the fourth value from the stack
  OP_REM_4 = FAM_POP | 0x0B, // remove the fifth value from the stack
  OP_REM_5 = FAM_POP | 0x0C, // remove the sixth value from the stack
  OP_REM_6 = FAM_POP | 0x0D, // remove the seventh value from the stack
  OP_REM_7 = FAM_POP | 0x0E, // remove the eighth value from the stack
  OP_REM_R = FAM_POP | 0x0F, // remove a range of values from the stack

  OP_DUP_0  = FAM_DUP | 0x00, // push a copy of the top value onto the stack
  OP_DUP_1  = FAM_DUP | 0x01, // push a copy of the second value onto the stack
  OP_DUP_2  = FAM_DUP | 0x02, // push a copy of the third value onto the stack
  OP_DUP_3  = FAM_DUP | 0x03, // push a copy of the fourth value onto the stack
  OP_DUP_4  = FAM_DUP | 0x04, // push a copy of the fifth value onto the stack
  OP_DUP_5  = FAM_DUP | 0x05, // push a copy of the sixth value onto the stack
  OP_DUP_6  = FAM_DUP | 0x06, // push a copy of the seventh value onto the stack
  OP_DUP_7  = FAM_DUP | 0x07, // push a copy of the eighth value onto the stack
  OP_DUP_8  = FAM_DUP | 0x08, // push a copy of the nineth value onto the stack
  OP_DUP_9  = FAM_DUP | 0x09, // push a copy of the tenth value onto the stack
  OP_DUP_10 = FAM_DUP | 0x0A, // push a copy of the elevneth value onto the stack
  OP_DUP_11 = FAM_DUP | 0x0B, // push a copy of the twelfth value onto the stack
  OP_DUP_12 = FAM_DUP | 0x0C, // push a copy of the thirteenth value onto the stack
  OP_DUP_13 = FAM_DUP | 0x0D, // push a copy of the fourteenth value onto the stack
  OP_DUP_14 = FAM_DUP | 0x0E, // push a copy of the fifteenth value onto the stack
  OP_DUP_15 = FAM_DUP | 0x0F, // push a copy of the sixteenth value onto the stack

  OP_INC_I = FAM_MATH | 0x00, // increment the top value on the stack as an integer
  OP_DEC_I = FAM_MATH | 0x01, // decrement the top value on the stack as an integer
  OP_ABS_I = FAM_MATH | 0x02, // force the top value on the stack to a positive integer value
  OP_NEG_I = FAM_MATH | 0x03, // negate the top value on the stack as an integer
  OP_ADD_I = FAM_MATH | 0x04, // add the top two values on the stack as integers
  OP_SUB_I = FAM_MATH | 0x05, // subtract the second value from the top of the stack as integers
  OP_MUL_I = FAM_MATH | 0x06, // multiply the top two values on the stack as integers
  OP_DIV_I = FAM_MATH | 0x07, // divide the top of the stack by the second value as integers
#if EVM_FLOAT_SUPPORT == 1
  OP_INC_F = FAM_MATH | 0x08, // increment the top value on the stack as a float
  OP_DEC_F = FAM_MATH | 0x09, // decrement the top value on the stack as a float
  OP_ABS_F = FAM_MATH | 0x0A, // force the top value on the stack to a positive float value
  OP_NEG_F = FAM_MATH | 0x0B, // negate the top value on the stack as a float
  OP_ADD_F = FAM_MATH | 0x0C, // add the top two values on the stack as floats
  OP_SUB_F = FAM_MATH | 0x0D, // subtract the second value from the top of the stack as floats
  OP_MUL_F = FAM_MATH | 0x0E, // multiply the top two values on the stack as floats
  OP_DIV_F = FAM_MATH | 0x0F, // divide the top of the stack by the second value as floats
#endif

  OP_LSH       = FAM_BITS | 0x00, // left shift top of stack by second value
  OP_RSH       = FAM_BITS | 0x01, // right shift top of stack by second value
  OP_AND       = FAM_BITS | 0x02, // bitwise AND the top of stack with the second value
  OP_OR        = FAM_BITS | 0x03, // bitwise OR the top of stack with the second value
  OP_XOR       = FAM_BITS | 0x04, // bitwise XOR the top of stack with the second value
  OP_INV       = FAM_BITS | 0x05, // bitwise invert value on the top of the stack
  OP_BOOL      = FAM_BITS | 0x06, // force canonicalization of top of stack to TRUE and FALSE
  OP_NOT       = FAM_BITS | 0x07, // negate the boolean value on the top of the stack
#if EVM_FLOAT_SUPPORT == 1
  OP_CONV_FI   = FAM_BITS | 0x08, // convert top of the stack from float to integer
  OP_CONV_FI_1 = FAM_BITS | 0x09, // convert the second value from float to integer
  OP_CONV_IF   = FAM_BITS | 0x0A, // convert top of the stack from integer to float
  OP_CONV_IF_1 = FAM_BITS | 0x0B, // convert the second value from integer to float
#endif

  OP_CMP_I0  = FAM_CMP | 0x00, // compare top of stack to 0 as an integer
  OP_CMP_I1  = FAM_CMP | 0x01, // compare top of stack to 1 as an integer
  OP_CMP_IN1 = FAM_CMP | 0x02, // compare to of stack to -1 as an integer
  OP_CMP_I   = FAM_CMP | 0x03, // compare the top of stack to the second value as integers
#if EVM_FLOAT_SUPPORT == 1
  OP_CMP_F0  = FAM_CMP | 0x04, // compare top of stack to 0.0 as a float
  OP_CMP_F1  = FAM_CMP | 0x05, // compare top of stack to 1.0 as a float
  OP_CMP_FN1 = FAM_CMP | 0x06, // compare to of stack to -1.0 as a float
  OP_CMP_F   = FAM_CMP | 0x07, // compare the top of stack to the second value as floats
#endif

  OP_JMP   = FAM_JMP | 0x00, // unconditional near jump
  OP_JLT   = FAM_JMP | 0x01, // near jump on less than comparison
  OP_JLE   = FAM_JMP | 0x02, // near jump on less than or equals comparison
  OP_JNE   = FAM_JMP | 0x03, // near jump on not equals comparison
  OP_JEQ   = FAM_JMP | 0x04, // near jump on equals comparison
  OP_JGE   = FAM_JMP | 0x05, // near jump on greater than or equals comparison
  OP_JGT   = FAM_JMP | 0x06, // near jump on greater than comparison
  OP_JTBL  = FAM_JMP | 0x07, // use top of stack as an integer for jump destination table lookup
  OP_LJMP  = FAM_JMP | 0x08, // unconditional far jump
  OP_LJLT  = FAM_JMP | 0x09, // far jump on less than comparison
  OP_LJLE  = FAM_JMP | 0x0A, // far jump on less than or equals comparison
  OP_LJNE  = FAM_JMP | 0x0B, // far jump on not equals comparison
  OP_LJEQ  = FAM_JMP | 0x0C, // far jump on equals comparison
  OP_LJGE  = FAM_JMP | 0x0D, // far jump on greater than or equals comparison
  OP_LJGT  = FAM_JMP | 0x0E, // far jump on greater than comparison
  OP_LJTBL = FAM_JMP | 0x0F, // use top of stack as an integer for jump destination table lookup

  OP_RET    = FAM_RET | 0x00, // use the top of stack as a program pointer and pop it
  OP_RET_1  = FAM_RET | 0x01, // use the second value as a program pointer and remove it
  OP_RET_2  = FAM_RET | 0x02, // use the third value as a program pointer and remove it
  OP_RET_3  = FAM_RET | 0x03, // use the fourth value as a program pointer and remove it
  OP_RET_4  = FAM_RET | 0x04, // use the fifth value as a program pointer and remove it
  OP_RET_5  = FAM_RET | 0x05, // use the sixth value as a program pointer and remove it
  OP_RET_6  = FAM_RET | 0x06, // use the seventh value as a program pointer and remove it
  OP_RET_7  = FAM_RET | 0x07, // use the eighth value as a program pointer and remove it
  OP_RET_8  = FAM_RET | 0x08, // use the nineth value as a program pointer and remove it
  OP_RET_9  = FAM_RET | 0x09, // use the tenth value as a program pointer and remove it
  OP_RET_10 = FAM_RET | 0x0A, // use the eleventh value as a program pointer and remove it
  OP_RET_11 = FAM_RET | 0x0B, // use the twelfth value as a program pointer and remove it
  OP_RET_12 = FAM_RET | 0x0C, // use the thirteenth value as a program pointer and remove it
  OP_RET_13 = FAM_RET | 0x0D, // use the fourteenth value as a program pointer and remove it
  OP_RET_14 = FAM_RET | 0x0E, // use the fifteenth value as a program pointer and remove it
  OP_RET_I  = FAM_RET | 0x0F, // use the next byte as the index for the program pointer
} opcode_t;


EVM_API const char *evmOpcodeToMnemonic(opcode_t);


#ifdef __cplusplus
}
#endif


#endif

