#ifndef GDEXT_EVM_CONFIG_H
#  define GDEXT_EVM_CONFIG_H

// enable float support
#define EVM_FLOAT_SUPPORT (1)
// allow a full compliment of builtin functions
#define EVM_MAX_BUILTINS (256)
// statically allocate the stack
#define EVM_STATIC_STACK (1)
// dynamically allocate main memory
#define EVM_STATIC_MEMORY (0)
// dynamically allocate program memory
#define EVM_STATIC_PROGRAM (0)
// only print internal eVM errors
#define EVM_LOG_LEVEL (2)

#endif /* GDEXT_EVM_CONFIG_H */

