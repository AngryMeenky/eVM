# eVM
## A tiny embeddable bytecode interpreter.

eVM is a simple stack based virtual machine designed for small size and ease of
integration rather than speed of execution. The configurability of eVM allows
unneeded features to be omitted during compilation to reduce the program and
memory requirements. Compiled code size can be as small as 2KB and there are
configurations that don't require memory allocation, thus making it possible to
provide scriptable capabilities on memory constrained microcontrollers.


## Tunable Features

| Feature                 | Range   | Description                                                    |
|-------------------------|---------|----------------------------------------------------------------|
| **EVM_FLOAT_SUPPORT**   | [0,1]   | Add support for floating point opcodes                         |
| **EVM_STATIC_STACK**    | [0,1]   | Select dynamically or statically allocated stack               |
| **EVM_STATIC_PROGRAM**  | [0,1]   | Select dynamically or statically allocated program             |
| **EVM_STATIC_MEMORY**   | [0,1]   | Select dynamically or statically allocated memory banks        |
| **EVM_MEMORY_BANKS**    | [0,256] | How many memory banks are available (0 removes memory opcodes) |
| **EVM_MEMORY_BANK_POW** | [10,16] | Set the size of the available memory banks (1KB to 64KB)       |
| **EVM_UNALIGNED_READS** | [0,1]   | Allow for unaligned multibyte reads (and writes)               |
| **EVM_MAX_BUILTINS**    | [0,256] | Set the number of bound functions available to eVM programs    |
| **EVM_LOG_LEVEL**       | [0,6]   | How verbose is the logging (0 disables logging)                |
