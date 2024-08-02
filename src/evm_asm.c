#include "evm/asm.h"
#include "evm/opcodes.h"

#include <math.h>
#include <ctype.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>


typedef enum evm_arg_e {
  ARG_NONE,   // no argument
  ARG_I8,     // one byte integer literal
  ARG_I16,    // two byte integer literal
  ARG_I24,    // three byte integer literal
  ARG_I32,    // four byte integer literal
  ARG_O1,     // optional one bit integer literal
  ARG_O3,     // optional three bit integer literal
  ARG_O4,     // optional four bit integer literal
  ARG_I4_O4,  // four bit integer literal and optional four bit integer literal
  ARG_O8,     // optional one byte integer literal
  ARG_LBL,    // a label literal
#if EVM_FLOAT_SUPPORT == 1
  ARG_F32,    // four byte floating point literal
  ARG_OF32    // optional four byte floating point literal
#endif
} evm_arg_t;


typedef enum evm_flags_e {
  INST_DIRECTIVE   = 1 << 2,
  INST_LABEL       = 1 << 3,
  INST_FINALIZED   = 1 << 4,
  INST_UNRESOLVED  = 1 << 5,
  INST_INVALID_ARG = 1 << 6,
  INST_MISSING_ARG = 1 << 7,
} evm_flags_t;


typedef enum evm_dir_e {
  DIR_BASE,
  DIR_NAME,
  DIR_DATA,
  DIR_TBL,
} evm_dir_t;


struct evm_directive_s;
typedef struct evm_directive_s evm_directive_t;


typedef int (*evm_handler_t)(const struct evm_directive_s *, evm_instruction_t *);


struct evm_directive_s {
  const char    *tag;
  evm_handler_t  process;
  evm_arg_t      arg;
  evm_dir_t      kind;
};


struct evm_mnemonic_s;
typedef struct evm_mnemonic_s evm_mnemonic_t;

struct evm_section_s;
typedef struct evm_section_s evm_section_t;

typedef int (*evm_serializer_t)(const struct evm_mnemonic_s *, evm_instruction_t *);


struct evm_mnemonic_s {
  const char       *tag;
  evm_arg_t         arg;
  opcode_t          op;
  evm_serializer_t  process;
};


typedef struct evm_label_s {
  struct evm_label_s *next;
  struct evm_label_s *prev;
  evm_section_t      *section;
  uint32_t            offset;
  uint32_t            id;
  char                name[];
} evm_label_t;


typedef struct evm_instruction_ref_s {
  struct evm_instruction_ref_s *next;
  evm_label_t                  *target;
  evm_instruction_t            *instruction;
  int32_t                       offset;
  int32_t                       size;
} evm_instruction_ref_t;


struct evm_section_s {
  evm_section_t         *next;
  evm_section_t         *prev;
  uint8_t               *contents;
  evm_label_t            labels;
  evm_instruction_ref_t *instructions;
  evm_instruction_ref_t *tail;
  uint32_t               base;
  uint32_t               length;
  uint32_t               capacity;
  char                   name[];
};


typedef struct evm_ptr_list_s {
  struct evm_ptr_list_s *prev;
  struct evm_ptr_list_s *next;
  void                  *ptr;
} evm_ptr_list_t;


struct evm_program_s {
  evm_section_t  sections;
  evm_ptr_list_t files;
  uint32_t       base;
  uint32_t       length;
  uint32_t       count;
};


static void               evmasmClearLabelList(evm_label_t *);
static void               evmasmClearFilesList(evm_ptr_list_t *);
static void               evmasmClearSectionList(evm_section_t *);
static void               evmasmClearInstructionList(evm_instruction_t *);
static void               evmasmAppendInstruction(evm_instruction_t *, evm_instruction_t *);
static evm_instruction_t *evmasmNewInstruction(const char *, const char *, const char *, uint32_t);
static evm_program_t     *evmasmNewProgram();
static void               evmasmDeleteProgram(evm_program_t *);
static const char        *evmasmCanonicalizeString(evm_ptr_list_t *, const char *);
static evm_section_t     *evmasmNewSection(const char *);
static void               evmasmDeleteSection(evm_section_t *);
static evm_section_t     *evmasmCanonicalizeSection(evm_section_t *, const char *);
static void               evmasmAddToSection(evm_section_t *, evm_instruction_t *);
static uint32_t           evmasmCalculateLikelySectionLength(evm_section_t *);
static evm_label_t       *evmasmNewLabel(const char *, uint32_t);
static evm_label_t       *evmasmAppendLabel(evm_section_t *, const char *);
static int                mnemonicCompare(const char *, const char *, const char *);


evm_assembler_t *evmasmAllocate() {
  return (evm_assembler_t *) calloc(1, sizeof(evm_assembler_t));
}


evm_assembler_t *evmasmInitialize(evm_assembler_t *evm) {
  if(evm) {
    evm_program_t *program = evmasmNewProgram();

    if(program) {
      memset((void *) evm, 0, sizeof(evm_assembler_t));
      evm->head.prev = &evm->head;
      evm->head.next = &evm->head;
      evm->output = program;
    }
    else {
      evmasmFinalize(evm);
      evm = NULL;
    }
  }

  return evm;
}


evm_assembler_t *evmasmFinalize(evm_assembler_t *evm) {
  if(evm) {
    evmasmClearInstructionList(&evm->head);
    if(evm->output) {
      evmasmDeleteProgram(evm->output);
    }
    memset((void *) evm, 0, sizeof(evm_assembler_t));
  }

  return evm;
}


void evmasmFree(evm_assembler_t *evm) {
  free(evm);
}


int evmasmParseFile(evm_assembler_t *evm, const char *name, FILE *fp) {
  int result = 0;

  if(evm && fp) {
    int line = 0;
    char buffer[BUFSIZ];

    while(fgets(&buffer[0], BUFSIZ, fp)) {
      result |= evmasmParseLine(evm, name, &buffer[0], ++line);
    }
  }
  else {
    result = -1;
  }

  return result;
}


static int evmDataDirective(const evm_directive_t *, evm_instruction_t *);
static int evmTextDirective(const evm_directive_t *, evm_instruction_t *);
static int evmAddressDirective(const evm_directive_t *, evm_instruction_t *);


// List of directives and associated arguments
static const evm_directive_t DIRECTIVES[] = {
  { ".name",   &evmTextDirective,    ARG_LBL,  DIR_NAME },
  { ".offset", &evmAddressDirective, ARG_I24,  DIR_BASE },
  { ".addr",   &evmTextDirective,    ARG_LBL,  DIR_TBL  },
  { ".db",     &evmDataDirective,    ARG_I8,   DIR_DATA },
  { ".dh",     &evmDataDirective,    ARG_I16,  DIR_DATA },
  { ".dw",     &evmDataDirective,    ARG_I32,  DIR_DATA },
#if EVM_FLOAT_SUPPORT == 1
  { ".df",     &evmDataDirective,    ARG_F32,  DIR_DATA },
#endif
  { NULL,      NULL,                 ARG_NONE, 0        },
};


static int evmPushSerializer(const evm_mnemonic_t *, evm_instruction_t *);
static int evmLabelSerializer(const evm_mnemonic_t *, evm_instruction_t *);
static int evmSimpleSerializer(const evm_mnemonic_t *, evm_instruction_t *);
static int evmCompareSerializer(const evm_mnemonic_t *, evm_instruction_t *);
static int evmOptionalSerializer(const evm_mnemonic_t *, evm_instruction_t *);


// List of mnemonics, associated arguments, and covered opcodes
static const evm_mnemonic_t MNEMONICS[] = {
  { "NOP",   ARG_NONE,  OP_NOP,     &evmSimpleSerializer   },
  { "CALL",  ARG_LBL,   OP_CALL,    &evmLabelSerializer    }, // OP_CALL, OP_LCALL
  { "BLTIN", ARG_I8,    OP_BCALL,   &evmSimpleSerializer   },
  { "YIELD", ARG_NONE,  OP_YIELD,   &evmSimpleSerializer   },
  { "HALT",  ARG_NONE,  OP_HALT,    &evmSimpleSerializer   },
  { "PUSH",  ARG_I32,   OP_PUSH_I0, &evmPushSerializer     }, // OP_PUSH_{I0,I1,IN1,8I,16I,24I,32I}
  { "SWAP",  ARG_NONE,  OP_SWAP,    &evmSimpleSerializer   },
  { "POP",   ARG_O3,    OP_POP_1,   &evmOptionalSerializer }, // OP_POP_{1,2,3,4,5,6,7,8}
  { "REM",   ARG_I4_O4, OP_REM_1,   &evmOptionalSerializer }, // OP_REM_{1,2,3,4,5,6,7,R}
  // OP_DUP_{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}
  { "DUP",   ARG_O4,    OP_DUP_0,   &evmOptionalSerializer },
  { "INC",   ARG_NONE,  OP_INC_I,   &evmSimpleSerializer   },
  { "DEC",   ARG_NONE,  OP_DEC_I,   &evmSimpleSerializer   },
  { "ABS",   ARG_NONE,  OP_ABS_I,   &evmSimpleSerializer   },
  { "NEG",   ARG_NONE,  OP_NEG_I,   &evmSimpleSerializer   },
  { "ADD",   ARG_NONE,  OP_ADD_I,   &evmSimpleSerializer   },
  { "SUB",   ARG_NONE,  OP_SUB_I,   &evmSimpleSerializer   },
  { "MUL",   ARG_NONE,  OP_MUL_I,   &evmSimpleSerializer   },
  { "DIV",   ARG_NONE,  OP_DIV_I,   &evmSimpleSerializer   },
  { "LSH",   ARG_NONE,  OP_LSH,     &evmSimpleSerializer   },
  { "RSH",   ARG_NONE,  OP_RSH,     &evmSimpleSerializer   },
  { "AND",   ARG_NONE,  OP_AND,     &evmSimpleSerializer   },
  { "OR",    ARG_NONE,  OP_OR,      &evmSimpleSerializer   },
  { "XOR",   ARG_NONE,  OP_XOR,     &evmSimpleSerializer   },
  { "INV",   ARG_NONE,  OP_INV,     &evmSimpleSerializer   },
  { "BOOL",  ARG_NONE,  OP_BOOL,    &evmSimpleSerializer   },
  { "NOT",   ARG_NONE,  OP_NOT,     &evmSimpleSerializer   },
  { "CMP",   ARG_O8,    OP_CMP_I0,  &evmCompareSerializer  }, // OP_CMP_{I0,I1,IN1,I}
  { "JMP",   ARG_LBL,   OP_JMP,     &evmLabelSerializer    },
  { "JLT",   ARG_LBL,   OP_JLT,     &evmLabelSerializer    },
  { "JLE",   ARG_LBL,   OP_JLE,     &evmLabelSerializer    },
  { "JNE",   ARG_LBL,   OP_JNE,     &evmLabelSerializer    },
  { "JEQ",   ARG_LBL,   OP_JEQ,     &evmLabelSerializer    },
  { "JGE",   ARG_LBL,   OP_JGE,     &evmLabelSerializer    },
  { "JGT",   ARG_LBL,   OP_JGT,     &evmLabelSerializer    },
  { "JTBL",  ARG_NONE,  OP_JTBL,    &evmSimpleSerializer   },
  { "LJMP",  ARG_LBL,   OP_JMP,     &evmLabelSerializer    },
  { "LJLT",  ARG_LBL,   OP_JLT,     &evmLabelSerializer    },
  { "LJLE",  ARG_LBL,   OP_JLE,     &evmLabelSerializer    },
  { "LJNE",  ARG_LBL,   OP_JNE,     &evmLabelSerializer    },
  { "LJEQ",  ARG_LBL,   OP_JEQ,     &evmLabelSerializer    },
  { "LJGE",  ARG_LBL,   OP_JGE,     &evmLabelSerializer    },
  { "LJGT",  ARG_LBL,   OP_JGT,     &evmLabelSerializer    },
  { "LJTBL", ARG_NONE,  OP_LJTBL,   &evmSimpleSerializer   },
  // OP_RET{,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_I}
  { "RET",   ARG_O8,    OP_RET,     &evmOptionalSerializer },
#if EVM_FLOAT_SUPPORT == 1
  { "PUSHF", ARG_F32,   OP_PUSH_F0, &evmPushSerializer     }, // OP_PUSH_{F0,F1,FN1,F}}
  { "INCF",  ARG_NONE,  OP_INC_F,   &evmSimpleSerializer   },
  { "DECF",  ARG_NONE,  OP_DEC_F,   &evmSimpleSerializer   },
  { "ABSF",  ARG_NONE,  OP_ABS_F,   &evmSimpleSerializer   },
  { "NEGF",  ARG_NONE,  OP_NEG_F,   &evmSimpleSerializer   },
  { "ADDF",  ARG_NONE,  OP_ADD_F,   &evmSimpleSerializer   },
  { "SUBF",  ARG_NONE,  OP_SUB_F,   &evmSimpleSerializer   },
  { "MULF",  ARG_NONE,  OP_MUL_F,   &evmSimpleSerializer   },
  { "DIVF",  ARG_NONE,  OP_DIV_F,   &evmSimpleSerializer   },
  { "CNVFI", ARG_O1,    OP_CONV_FI, &evmOptionalSerializer }, // OP_CONV_{FI,FI_1}
  { "CNVIF", ARG_O1,    OP_CONV_IF, &evmOptionalSerializer }, // OP_CONV_{IF,IF_1}
  { "CMPF",  ARG_OF32,  OP_CMP_F0,  &evmCompareSerializer  }, // OP_CMP_{F0,F1,FN1,F}
#endif

  { NULL,    ARG_NONE,  OP_NOP,     NULL }, // "null" terminator at the end of the array
};


int evmasmParseLine(evm_assembler_t *evm, const char *name, const char *line, int num) {
  const char *start = line;
  const char *end = &line[strlen(line)];
  const char *cur;
  int result = 0;

  // trim leading whitespace
  while(start < end && isspace(*start)) { ++start; }
  // trim trailing whitespace
  while(start < &end[-1] && isspace(end[-1])) { --end; }
  // remove comments
  for(cur = start; cur < end; ++cur) {
    if(*cur == ';') {
      --cur;
      // remove whitespace from before the comment
      while(cur >= start && isspace(*cur)) { --cur; }
      end = &cur[1];
      break;
    }
  }

  // empty string check
  if(start != end) {
    name = evmasmCanonicalizeString(&evm->output->files, name);

    if(*start == '.') {
      const evm_directive_t *directive;
      int handled = 0;

      // parse directives
      for(directive = &DIRECTIVES[0]; directive->tag; ++directive) {
        if(!mnemonicCompare(&directive->tag[0], start, end)) {
          evm_instruction_t *inst = evmasmNewInstruction(name, start, end, num);
          evmasmAppendInstruction(&evm->head, inst);
          result = directive->process(directive, inst);
          handled = -1;
          break;
        }
      }

      if(!handled) {
        EVM_ERRORF("Unknown directive on line %d: %s", num, line);
        result = -1;
      }
    }
    else if(end[-1] == ':') {
      // process labels
      evm_instruction_t *inst = evmasmNewInstruction(name, start, end - 1, num);
      evmasmAppendInstruction(&evm->head, inst);
      inst->flags |= INST_LABEL;
    }
    else {
      const evm_mnemonic_t *mnemonic;
      int handled = 0;

      // parse instructions
      for(mnemonic = &MNEMONICS[0]; mnemonic->tag; ++mnemonic) {
        if(!mnemonicCompare(&mnemonic->tag[0], start, end)) {
          evm_instruction_t *inst = evmasmNewInstruction(name, start, end, num);
          evmasmAppendInstruction(&evm->head, inst);
          result = mnemonic->process(mnemonic, inst);
          handled = -1;
          break;
        }
      }

      if(!handled) {
        EVM_ERRORF("Unexpected input on line %d: %s", num, line);
        result = -1;
      }
    }
  }

  return result;
}


int evmasmValidateProgram(evm_assembler_t *evm) {
  int result = 0;

  if(evm) {
    evm_ptr_list_t     list;
    evm_program_t     *prog = evm->output;
    evm_instruction_t *insts = &evm->head;
    evm_instruction_t *inst;
    evm_section_t     *sects = &prog->sections;
    evm_section_t     *sect = NULL;

    evm->length = 0;

    // build the sections based on instruction stream
    for(inst = insts->next; inst != insts; inst = inst->next) {
      if(inst->flags & (INST_MISSING_ARG | INST_INVALID_ARG)) {
        result |= 1; // missing or bad argument on a specific instruction/directive
        EVM_ERRORF(
          "Invalid instruction in %s on line %d: %s",
          inst->file, inst->line, &inst->text[0]
        );
        continue;
      }

      if(inst->flags & INST_DIRECTIVE) {
        switch((evm_dir_t) inst->binary[0]) {
          case DIR_BASE:
            // set the base for the current section
            if(sect) {
              sect->base = inst->binary[1] | (inst->binary[2] << 8) | (inst->binary[3] << 16);
            }
            else {
              EVM_ERRORF(
                "Section not yet specified in %s on line %d: %s",
                inst->file, inst->line, &inst->text[0]
              );
              result |= 8;
            }
          break;

          case DIR_NAME:
            // create a new section or select a previous section with the given name
            sect = evmasmCanonicalizeSection(sects, &inst->text[inst->binary[1]]);
          break;

          case DIR_DATA:
            // data directive insert the data into the instruction stream
            if(sect) {
              evmasmAddToSection(sect, inst);
            }
            else {
              EVM_ERRORF(
                "Section not yet specified in %s on line %d: %s",
                inst->file, inst->line, &inst->text[0]
              );
              result |= 8;
            }
          break;

          case DIR_TBL:
            // add the jump table entry to the current table
            if(sect) {
              evmasmAddToSection(sect, inst);

              if(!sect->tail->size) {
                EVM_ERRORF(
                  "Headerless jump table entry in %s on line %d: %s",
                  inst->file, inst->line, &inst->text[0]
                );
                result |= 4096;
              }
            }
            else {
              EVM_ERRORF(
                "Section not yet specified in %s on line %d: %s",
                inst->file, inst->line, &inst->text[0]
              );
              result |= 8;
            }
          break;
        }
      }
      else if(inst->flags & INST_LABEL) {
        // add the label to the current section
        if(sect) {
          evm_label_t *label = evmasmAppendLabel(sect, &inst->text[inst->binary[1]]);
          if(label) {
            label->offset = evmasmCalculateLikelySectionLength(sect);
          }
          else {
            EVM_ERRORF(
              "Failure to create label in %s on line %d: %s",
              inst->file, inst->line, &inst->text[0]
            );
            result |= 16;
          }
        }
        else {
          EVM_ERRORF(
            "Section not yet specified in %s on line %d: %s",
            inst->file, inst->line, &inst->text[0]
          );
          result |= 8;
        }
      }
      else {
        // assign each instruction to the correct section
        if(sect) {
          evmasmAddToSection(sect, inst);
        }
        else {
          EVM_ERRORF(
            "Section not yet specified in %s on line %d: %s",
            inst->file, inst->line, &inst->text[0]
          );
          result |= 8;
        }
      }
    }

    // ensure no duplicate labels
    list.prev = &list;
    list.next = &list;
    for(sect = sects->next; sect != sects; sect = sect->next) {
      evm_label_t *labels = &sect->labels;
      evm_label_t *label;

      for(label = labels->next; label != labels; label = label->next) {
        evm_ptr_list_t *node;

        for(node = list.next; node != &list; node = node->next) {
          if(!strcmp(&label->name[0], (const char *) node->ptr)) {
            break; // found duplicate
          }
        }

        if(node != &list) {
          // report error
          EVM_ERRORF("Duplicate label: %s", &label->name[0]);
          result |= 2;
        }
        else {
          evmasmCanonicalizeString(&list, &label->name[0]);
        }
      }
    }

    // ensure all jmp targets are resolved
    for(sect = sects->next; sect != sects; sect = sect->next) {
      evm_instruction_ref_t *ref;

      for(ref = sect->instructions; ref; ref = ref->next) {
        inst = ref->instruction;

        if(inst->flags & INST_UNRESOLVED) {
          evm_section_t *section;
          const char *name = (char *) &inst->text[inst->binary[1]];
         
          for(section = sects->next; section != sects; section = section->next) {
            evm_label_t *labels = &sect->labels;
            evm_label_t *label;

            for(label = labels->next; label != labels; label = label->next) {
              if(!strcmp(&label->name[0], name)) {
                // found target label
                ref->target = label;
                section = sects->prev;
                break;
              }
            }
          }

          if(ref->target) {
            // mark as resolved and finalized
            inst->flags &= ~INST_UNRESOLVED;
            inst->flags |=  INST_FINALIZED;
          }
          else {
            // report error
            EVM_ERRORF(
              "Missing label in %s on line %d: %s",
              inst->file, inst->line, &inst->text[0]
            );
            result |= 4;
          }
        }
      }
    }

    // only process further if there have been no errors
    if(!result) {
      // sort the sections on base address
      for(sect = sects->next; sect != sects; sect = sect->next) {
        evm_section_t *min = sect;
        evm_section_t *test;

        // find the minimum base offset
        for(test = sect->next; test != sects; test = test->next) {
          if(test->base < min->base) {
            min = test;
          }
        }

        // swap if required
        if(sect != min) {
          evm_section_t *ptrs[4] = { sect->prev, sect->next, min->prev, min->next };

          // point min at sect's neighbors
          min->prev = ptrs[0];
          min->next = ptrs[1];
          min->prev->next = min;
          min->next->prev = min;

          // point sect at min's neighbors
          sect->prev = ptrs[2];
          sect->next = ptrs[3];
          sect->prev->next = sect;
          sect->next->prev = sect;
        }
      }

      // serialize the instructions into their respective sections
      for(sect = sects->next; sect != sects; sect = sect->next) {
        sect->capacity = evmasmCalculateLikelySectionLength(sect);
        sect->length = 0;

        if(sect->contents) {
          free(sect->contents); // avoid memory leaks
        }

        if((sect->contents = calloc(sect->capacity, 1))) {
          evm_instruction_ref_t *ref;
          evm_label_t *target;
          enum { INVALID, SHORT, LONG } mode = INVALID;
          int delta, branches = 0, entries = 0, tbl_off = 0;

          for(ref = sect->instructions; ref; ref = ref->next) {
            inst = ref->instruction;

            if(inst->flags & INST_DIRECTIVE) { // handle the data/address directives
              switch(inst->binary[0]) {
                case DIR_DATA:
                  memcpy(&sect->contents[sect->length], &inst->binary[1], ref->size);
                  sect->length += ref->size;
                break;

                case DIR_TBL:
                  switch(mode) {
                    case SHORT:
                      target = ref->target;
                      delta = (target->section->base + target->offset) - tbl_off;

                      if(-128 <= delta && delta <= 127) {
                        ++entries;
                        sect->contents[branches]++;
                        sect->contents[sect->length++] = (int8_t) delta;
                      }
                      else {
                        // report error
                        EVM_ERRORF(
                          "Jump too far in %s on line %d: %s",
                          inst->file, inst->line, &inst->text[0]
                        );
                        result |= 64;
                      }
                    break;

                    case LONG:
                      target = ref->target;
                      delta = (target->section->base + target->offset) - tbl_off;

                      if(-32768 <= delta && delta <= 32767) {
                        ++entries;
                        sect->contents[branches]++;
                        sect->contents[sect->length++] =  delta       & 0xFF;
                        sect->contents[sect->length++] = (delta >> 8) & 0xFF;
                      }
                      else {
                        // report error
                        EVM_ERRORF(
                          "Jump too far in %s on line %d: %s",
                          inst->file, inst->line, &inst->text[0]
                        );
                        result |= 64;
                      }
                    break;

                    default:
                      // report error
                      EVM_ERRORF(
                        "Table entry without preceding jump instruction in %s on line %d: %s",
                        inst->file, inst->line, &inst->text[0]
                      );
                      result |= 128;
                    break;
                  }
                break;

                default:
                  // nothing to do
                break;
              }
            }
            else { // handle normal instructions
              switch(inst->binary[0]) {
                default:
                  if(mode != INVALID && !entries) {
                    EVM_ERRORF(
                      "Empty jump table detected in %s on line %d: %s",
                      inst->file, inst->line, &inst->text[0]
                    );
                    result |= 2048;
                  }
                  mode = INVALID;
                  memcpy(&sect->contents[sect->length], &inst->binary[0], inst->count);
                  sect->length += inst->count;
                break;

                // short jumps
                case OP_JMP:
                case OP_JLT:
                case OP_JLE:
                case OP_JNE:
                case OP_JEQ:
                case OP_JGE:
                case OP_JGT:
                  mode = INVALID;
                  target = ref->target;
                  delta = (target->section->base + target->offset) - (sect->base + ref->offset);

                  if(-128 <= delta && delta <= 127) {
                    sect->contents[sect->length++] = inst->binary[0];
                    sect->contents[sect->length++] = (int8_t) delta;
                  }
                  else {
                    // report error
                    EVM_ERRORF(
                      "Jump too far in %s on line %d: %s",
                      inst->file, inst->line, &inst->text[0]
                    );
                    result |= 64;
                  }
                break;

                // short jump table
                case OP_JTBL:
                  sect->contents[sect->length++] = inst->binary[0];
                  sect->contents[branches = sect->length++] = -1;
                  tbl_off = sect->base + ref->offset;
                  entries = 0;
                  mode = SHORT;
                break;

                // long jumps
                case OP_LJMP:
                case OP_LJLT:
                case OP_LJLE:
                case OP_LJNE:
                case OP_LJEQ:
                case OP_LJGE:
                case OP_LJGT:
                case OP_CALL:
                  mode = INVALID;
                  target = ref->target;
                  delta = (target->section->base + target->offset) - (sect->base + ref->offset);

                  if(-32768 <= delta && delta <= 32767) {
                    sect->contents[sect->length++] = inst->binary[0];
                    sect->contents[sect->length++] =  delta       & 0xFF;
                    sect->contents[sect->length++] = (delta >> 8) & 0xFF;
                  }
                  else {
                    // report error
                    EVM_ERRORF(
                      "Jump too far in %s on line %d: %s",
                      inst->file, inst->line, &inst->text[0]
                    );
                    result |= 64;
                  }
                break;

                // long jump table
                case OP_LJTBL:
                  sect->contents[sect->length++] = inst->binary[0];
                  sect->contents[branches = sect->length++] = -1;
                  tbl_off = sect->base + ref->offset;
                  entries = 0;
                  mode = LONG;
                break;

                // extremely long jump
                case OP_LCALL:
                  target = ref->target;
                  delta = (target->section->base + target->offset) - (sect->base + ref->offset);

                  if(-8388608 <= delta && delta <= 8388607) {
                    sect->contents[sect->length++] = inst->binary[0];
                    sect->contents[sect->length++] =  delta        & 0xFF;
                    sect->contents[sect->length++] = (delta >>  8) & 0xFF;
                    sect->contents[sect->length++] = (delta >> 16) & 0xFF;
                  }
                  else {
                    // report error
                    EVM_ERRORF(
                      "Jump too far in %s on line %d: %s",
                      inst->file, inst->line, &inst->text[0]
                    );
                    result |= 64;
                  }
                break;
              }
            }
          }
        }
        else {
          // report error
          EVM_ERRORF("Unable to allocate memory for section: %s", &sect->name[0]);
          result |= 32;
          break;
        }
      }
    }

    if(!result) {
      // ensure that no sections overlap
      for(sect = sects->next; ; sect = sect->next) {
        if(sect->next != sects) {
          if(!sect->length) {
            // report error
            EVM_ERRORF("Section %s is empty", &sect->name[0]);
            result |= 256;
          }
          else if((sect->base + sect->length) > sect->next->base) {
            // report error
            EVM_ERRORF("Sections %s and %s overlap", &sect->name[0], &sect->next->name[0]);
            result |= 512;
          }
        }
        else {
          break;
        }
      }
    }

    if(!result) {
      evm_instruction_ref_t *ref;

      // ensure first byte is a valid instruction and not data
      for(ref = sects->next->instructions; ref; ref = ref->next) {
        inst = ref->instruction;

        if(inst->flags & INST_DIRECTIVE) {
          if(inst->binary[0] == DIR_DATA || inst->binary[0] == DIR_TBL) {
            // report error
            EVM_ERRORF(
              "First byte is not an instruction in %s from line %d: %s",
              inst->file, inst->line, &inst->text[0]
            );
            result |= 1024;
          }
        }
        else if(inst->flags & INST_LABEL) {
          // ignore normal lables
        }
        else {
          break; // any regular instruction counts
        }
      }
    }

    if(!result) {
      // update the program length
      evm->length = sects->prev->base + sects->prev->length;
    }
  }
  else {
    result = -1;
  }

  return result;
}


uint32_t evmasmProgramSize(const evm_assembler_t *evm) {
  return evm ? evm->length : 0;
}


uint32_t evmasmProgramToBuffer(const evm_assembler_t *evm, uint8_t *buf, uint32_t max) {
  if(evm && buf) {
    if(evm->length || !evmasmValidateProgram((evm_assembler_t *) evm)) {
      if(evm->length <= max) {
        evm_program_t *prog = evm->output;
        evm_section_t *s;

        // zero the buffer, empty space between sections will remain zeroed
        memset(buf, 0, evm->length);
        // convert section list to flat buffer
        for(s = prog->sections.next; s != &prog->sections; s = s->next) {
          memcpy(&buf[s->base], s->contents, s->length);
        }

        return evm->length;
      }
    }
  }

  return 0;
}


int evmasmProgramToFile(const evm_assembler_t *evm, FILE *fp) {
  int result = -1;
  if(evm && fp) {
    if(evm->length || !evmasmValidateProgram((evm_assembler_t *) evm)) {
      uint8_t *buffer = malloc(evm->length);

      if(buffer) {
        uint32_t size = evmasmProgramToBuffer(evm, buffer, evm->length);

        if(size) {
          result = fwrite(buffer, 1, size, fp) == size ? 0 : -1;
        }
        else {
          result = -1;
        }

        free(buffer);
      }
      else {
        result = -1;
      }
    }
    else {
      result = -1;
    }
  }

  return result;
}


static void evmasmClearInstructionList(evm_instruction_t *list) {
  evm_instruction_t *node, *tmp;

  for(node = list->next; node != list; node = tmp) {
    tmp = node->next;
    free(node);
  }

  list->prev = list;
  list->next = list;
}


static void evmasmAppendInstruction(evm_instruction_t *list, evm_instruction_t *item) {
  // point at the new peers
  item->next = list;
  item->prev = list->prev;

  // make new peers consistent
  list->prev->next = item;
  list->prev = item;
}


static void evmasmClearSectionList(evm_section_t *list) {
  evm_section_t *node, *tmp;

  for(node = list->next; node != list; node = tmp) {
    tmp = node->next;
    evmasmDeleteSection(node);
  }

  list->prev = list;
  list->next = list;
}


static void evmasmClearLabelList(evm_label_t *list) {
  evm_label_t *node, *tmp;

  for(node = list->next; node != list; node = tmp) {
    tmp = node->next;
    free(node);
  }

  list->prev = list;
  list->next = list;
}


static void evmasmClearFilesList(evm_ptr_list_t *list) {
  evm_ptr_list_t *node, *tmp;

  for(node = list->next; node != list; node = tmp) {
    tmp = node->next;
    free(node);
  }

  list->prev = list;
  list->next = list;
}


static evm_instruction_t *
evmasmNewInstruction(const char *name, const char *start, const char *end, uint32_t line) {
  evm_instruction_t *inst = calloc(1, (end - start) + sizeof(evm_instruction_t) + 1U);
  if(inst) {
    inst->file = name;
    inst->line = line;
    memcpy(&inst->text[0], start, end - start);
    inst->text[end - start] = '\0';
  }

  return inst;
}


static evm_program_t *evmasmNewProgram() {
  evm_program_t *prog = calloc(1, sizeof(evm_program_t));

  if(prog) {
    prog->sections.prev = &prog->sections;
    prog->sections.next = &prog->sections;
    prog->files.prev = &prog->files;
    prog->files.next = &prog->files;
  }

  return prog;
}


static void evmasmDeleteProgram(evm_program_t *prog) {
  if(prog) {
    evmasmClearSectionList(&prog->sections);
    evmasmClearFilesList(&prog->files);
    free(prog);
  }
}


const char *evmasmCanonicalizeString(evm_ptr_list_t *list, const char *str) {
  evm_ptr_list_t *node;

  for(node = list->next; node != list; node = node->next) {
    if(!strcmp(str, (const char *) node->ptr)) {
      return (const char *) node->ptr;
    }
  }

  node = calloc(1, sizeof(evm_ptr_list_t) + strlen(str) + 1);
  if(node) {
    node->ptr = strcpy((char *) &node[1], str);

    node->prev = list;
    node->next = list->next;
    list->next->prev = node;
    list->next = node;
  }

  return NULL; // failure!
}


static evm_section_t *evmasmNewSection(const char *name) {
  evm_section_t *section = calloc(1, sizeof(evm_section_t) + strlen(name) + 1U);

  if(section) {
    section->labels.prev = &section->labels;
    section->labels.next = &section->labels;
    if((section->contents = (uint8_t *) calloc(1, 512U))) {
      section->capacity = 512U;
    }
    strcpy(&section->name[0], name);
  }

  return section;
}


static void evmasmDeleteSection(evm_section_t *section) {
  if(section) {
    evmasmClearLabelList(&section->labels);
    if(section->contents) {
      free(section->contents);
    }
    free(section);
  }
}


static evm_section_t *evmasmCanonicalizeSection(evm_section_t *list, const char *name) {
  evm_section_t *section = NULL;

  if(list) {
    for(section = list->next; section != list; section = section->next) {
      if(!strcmp(name, &section->name[0])) {
        break; // found it
      }
    }

    if(section == list) {
      // append to list if not found
      section = evmasmNewSection(name);
      section->prev = list->prev;
      list->prev->next = section;
      section->next = list;
      list->prev = section;
    }
  }
  else {
    if((section = evmasmNewSection(name))) {
      section->prev = section;
      section->next = section;
    }
  }

  return section;
}


static void evmasmAddToSection(evm_section_t *section, evm_instruction_t *inst) {
  if(section) {
    evm_instruction_ref_t *ref = calloc(1, sizeof(evm_instruction_ref_t));

    if(ref) {
      if(section->instructions) {
        ref->offset = section->tail->offset + section->tail->size;
        section->tail->next = ref;
        section->tail = ref;
      }
      else {
        section->instructions = section->tail = ref;
      }

      ref->instruction = inst;

      if(!(inst->flags & INST_DIRECTIVE)) {
        // assume the simple serializer has done a good job except for jumps and calls
        if((inst->binary[0] & 0xF0) == FAM_JMP) {
          switch(inst->binary[0]) {
            // short jumps
            case OP_JMP:
            case OP_JLT:
            case OP_JLE:
            case OP_JNE:
            case OP_JEQ:
            case OP_JGE:
            case OP_JGT:
              ref->size = 2;
            break;

            // jump tables
            case OP_JTBL:
            case OP_LJTBL:
              ref->size = 2; // doesn't include jump table entries
            break;

            // long jumps
            case OP_LJMP:
            case OP_LJLT:
            case OP_LJLE:
            case OP_LJNE:
            case OP_LJEQ:
            case OP_LJGE:
            case OP_LJGT:
              ref->size = 3;
            break;
          }
        }
        // wait for all labels to be resolved to make a selection about the size of the jump
        else if(inst->binary[0] == OP_CALL) {
            ref->size = 3; // long jump
        }
        else if(inst->binary[0] == OP_LCALL) {
            ref->size = 4; // longest possible jump
        }
        else {
          ref->size = inst->count; // assume the serializer got it right
        }
      }
      else if(inst->binary[0] == DIR_DATA) {
        ref->size = inst->count - 1;
      }
      else if(inst->binary[0] == DIR_TBL) {
        // search back for jump table instruction
        evm_instruction_t *i = inst->prev, *head = section->instructions->instruction->prev;
        ref->size = 0; // invalid size until resolved

        while(i != head) {
          if(i->binary[0] == OP_JTBL) {
            ref->size = 1; // short jumps only
            break;
          }
          else if(i->binary[0] == OP_LJTBL) {
            ref->size = 2; // long jumps
            break;
          }

          i = i->prev;
        }
      }
      else {
        // most directives and invalid instructions don't get added to the binary
        ref->size = 0;
      }
    }
  }
}


static uint32_t evmasmCalculateLikelySectionLength(evm_section_t *section) {
  uint32_t length = 0U;

  if(section && section->tail) {
    evm_instruction_ref_t *inst = section->tail;

    length = inst->offset + inst->size;
  }

  return length;
}


static evm_label_t *evmasmNewLabel(const char *name, uint32_t id) {
  evm_label_t *label = calloc(1, sizeof(evm_label_t) + strlen(name) + 1U);

  if(label) {
    label->offset = 0xFF000000U; // maximum section size is 24bits
    label->id = id;
    strcpy(&label->name[0], name);
  }

  return label;
}


static evm_label_t *evmasmAppendLabel(evm_section_t *section, const char *name) {
  evm_label_t *label = NULL;
 
  if(section) {
    if((label = evmasmNewLabel(name, ++section->labels.id))) {
      label->section = section;
      label->next = &section->labels;
      label->prev = section->labels.prev;
      section->labels.prev->next = label;
      section->labels.prev = label;
    }
  }

  return label;
}


static int mnemonicCompare(const char *tag, const char *start, const char *end) {
  int result = 0;

  // more or less standard strcmp
  while(!result && start != end && *tag) {
    result = tolower(*tag++) - tolower(*start++);
  }

  // on match ensure that neither string is longer than the other
  if(!result) {
    if(*tag) {
      result = 1; // tag is longer than the mnemonic
    }
    else if(start != end && !(isspace(*start) || *start == ';')) {
      result = -1; // mnemonic is longer than the tag
    }
  }

  return result;
}


static int evmDataDirective(const evm_directive_t *d, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = d->kind;
  i->flags |= INST_DIRECTIVE;
  i->count = 1;

  if(d->arg == ARG_I8) {
    int32_t operand;

    if(sscanf(&i->text[0], "%*s %d", &operand) == 1) {
      if(-128 <= operand && operand <= 255) {
        i->binary[1] = (uint8_t) (operand & 0xFF);
        i->count += 1;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (-128 <= %d <= 255)", &d->tag[0], operand);
      }
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &d->tag[0]);
    }
  }
  else if(d->arg == ARG_I16) {
    int32_t operand;

    if(sscanf(&i->text[0], "%*s %d", &operand) == 1) {
      if(-32768 <= operand && operand <= 65535) {
        i->binary[1] = (uint8_t) ( operand       & 0xFF);
        i->binary[2] = (uint8_t) ((operand >> 8) & 0xFF);
        i->count += 2;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (-32768 <= %d <= 65535)", &d->tag[0], operand);
      }
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &d->tag[0]);
    }
  }
  else if(d->arg == ARG_I32) {
    int64_t operand;

    if(sscanf(&i->text[0], "%*s %ld", &operand) == 1) {
      if(-2147483648 <= operand && operand <= 4294967295) {
        i->binary[1] = (uint8_t) ( operand        & 0xFF);
        i->binary[2] = (uint8_t) ((operand >>  8) & 0xFF);
        i->binary[3] = (uint8_t) ((operand >> 16) & 0xFF);
        i->binary[4] = (uint8_t) ((operand >> 24) & 0xFF);
        i->count += 4;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF(
          "Operand out of bounds for %s (-2147483648 <= %ld <= 4294967295)", &d->tag[0], operand
        );
      }
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &d->tag[0]);
    }
  }
#if EVM_FLOAT_SUPPORT == 1
  else if(d->arg == ARG_F32) {
    float operand;

    if(sscanf(&i->text[0], "%*s %f", &operand) == 1) {
      uint32_t bits = *(uint32_t *) &operand;
      i->binary[1] = (uint8_t) ( bits        & 0xFF);
      i->binary[2] = (uint8_t) ((bits >>  8) & 0xFF);
      i->binary[3] = (uint8_t) ((bits >> 16) & 0xFF);
      i->binary[4] = (uint8_t) ((bits >> 24) & 0xFF);
      i->count += 4;
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &d->tag[0]);
    }
  }
#endif
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &d->tag[0]);
  }

  return result;
}


static int evmTextDirective(const evm_directive_t *d, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = d->kind;
  i->flags |= INST_DIRECTIVE | (d->kind == DIR_TBL ? INST_UNRESOLVED : 0);
  i->count = 1;

  if(d->arg == ARG_LBL) {
    const char *ptr = &i->text[0];
    // skip the mnemonic
    while(*ptr && !isspace(*ptr)) { ++ptr; }
    // skip the whitespace
    while(*ptr && isspace(*ptr)) { ++ptr; }

    if(*ptr) {
      i->binary[1] = (int8_t) (ptr - &i->text[0]);
      i->flags |= INST_LABEL | (d->kind != DIR_TBL ? INST_FINALIZED : 0);
      i->count++;
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG | INST_LABEL;
      EVM_ERRORF("Missing operand for %s", &d->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &d->tag[0]);
  }

  return result;
}


static int evmAddressDirective(const evm_directive_t *d, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = d->kind;
  i->flags |= INST_DIRECTIVE;
  i->count = 1;

  if(d->arg == ARG_I24) {
    int32_t operand;

    if(sscanf(&i->text[0], "%*s %d", &operand) == 1) {
      if(0 <= operand && operand <= 16777215) {
        i->binary[1] = (uint8_t) ( operand        & 0xFF);
        i->binary[2] = (uint8_t) ((operand >>  8) & 0xFF);
        i->binary[3] = (uint8_t) ((operand >> 16) & 0xFF);
        i->count += 3;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (0 <= %d <= 16777215)", &d->tag[0], operand);
      }
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &d->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &d->tag[0]);
  }

  return result;
}


static int evmPushSerializer(const evm_mnemonic_t *m, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = m->op;
  i->count = 1;

  if(m->arg == ARG_I32) {
    int32_t operand;

    if(sscanf(&i->text[0], "%*s %d", &operand) == 1) {
      // handle the special cases
      if(-1 <= operand  && operand <= 1) {
        switch(operand) {
          case -1:
            i->binary[0] = m->op + 2;
          break;

          case 0:
            i->binary[0] = m->op;
          break;

          case 1:
            i->binary[0] = m->op + 1;
          break;
        }
      }
      else if(-128 <= operand  && operand <= 127) { // single byte
        i->binary[0] = m->op + 3;
        i->binary[1] = (uint8_t) (operand & 0xFF);
        i->count++;
      }
      else if(-32768 <= operand  && operand <= 32767) { // two bytes
        i->binary[0] = m->op + 4;
        i->binary[1] = (uint8_t) (operand & 0xFF);
        i->binary[2] = (uint8_t) (operand >>   8);
        i->count += 2;
      }
      else if(-8388608 <= operand  && operand <= 8388607) { // three bytes
        i->binary[0] = m->op + 5;
        i->binary[1] = (uint8_t) ( operand        & 0xFF);
        i->binary[2] = (uint8_t) ((operand >>  8) & 0xFF);
        i->binary[3] = (uint8_t) ((operand >> 16) & 0xFF);
        i->count += 3;
      }
      else { // four bytes
        i->binary[0] = m->op + 6;
        i->binary[1] = (uint8_t) ( operand        & 0xFF);
        i->binary[2] = (uint8_t) ((operand >>  8) & 0xFF);
        i->binary[3] = (uint8_t) ((operand >> 16) & 0xFF);
        i->binary[4] = (uint8_t) ((operand >> 24) & 0xFF);
        i->count += 4;
      }

      i->flags |= INST_FINALIZED;
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
#if EVM_FLOAT_SUPPORT == 1
  else if(m->arg == ARG_F32) {
    float operand;

    if(sscanf(&i->text[0], "%*s %f", &operand) == 1) {
      // handle the special cases
      if(operand == -1.0f) {
        i->binary[0] = m->op + 2;
      }
      else if(operand == 0.0f) {
        i->binary[0] = m->op;
      }
      else if(operand == 1.0f) {
        i->binary[0] = m->op + 1;
      }
      else { // four bytes
        int32_t bits = *(int32_t *) &operand;
        i->binary[0] = m->op + 3;
        i->binary[1] = (uint8_t) ( bits        & 0xFF);
        i->binary[2] = (uint8_t) ((bits >>  8) & 0xFF);
        i->binary[3] = (uint8_t) ((bits >> 16) & 0xFF);
        i->binary[4] = (uint8_t) ((bits >> 24) & 0xFF);
        i->count += 4;
      }
      i->flags |= INST_FINALIZED;
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
#endif
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }

  return result;
}


static int evmLabelSerializer(const evm_mnemonic_t *m, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = m->op;
  i->count = 1;

  if(m->arg == ARG_LBL) {
    const char *ptr = &i->text[0];
    // skip the mnemonic
    while(*ptr && !isspace(*ptr)) { ++ptr; }
    // skip the whitespace
    while(*ptr && isspace(*ptr)) { ++ptr; }

    if(*ptr) {
      i->binary[1] = (int8_t) (ptr - &i->text[0]);
      i->flags |= INST_UNRESOLVED;
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }

  return result;
}


static int evmSimpleSerializer(const evm_mnemonic_t *m, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = m->op;
  i->count = 1;

  if(m->arg == ARG_NONE) {
    i->flags |= INST_FINALIZED;
  }
  else if(m->arg == ARG_I8) {
    int32_t operand;

    if(sscanf(&i->text[0], "%*s %d", &operand) == 1) {
      if(-128 <= operand  && operand <= 127) {
        i->binary[1] = (int8_t) operand;
        i->flags |= INST_FINALIZED;
        i->count++;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (-128 <= %d <= 127)", &m->tag[0], operand);
      }
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }

  return result;
}


static int evmCompareSerializer(const evm_mnemonic_t *m, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = m->op;
  i->count = 1;

  if(m->arg == ARG_O8) {
    int32_t operand;

    if(sscanf(&i->text[0], "%*s %d", &operand) == 1) {
      if(-1 <= operand  && operand <= 1) {
        switch(operand) {
          case -1:
            i->binary[0] = m->op + 2;
          break;

          case 0:
            i->binary[0] = m->op;
          break;

          case 1:
            i->binary[0] = m->op + 1;
          break;
        }

        i->flags |= INST_FINALIZED;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (-1 <= %d <= 1)", &m->tag[0], operand);
      }
    }
    else {
      i->binary[0] = m->op + 3;
      i->flags |= INST_FINALIZED;
    }
  }
#if EVM_FLOAT_SUPPORT == 1
  else if(m->arg == ARG_OF32) {
    float operand;

    if(sscanf(&i->text[0], "%*s %f", &operand) == 1) {
      if(fabsf(operand + 1.0f) < FLT_EPSILON) {
        i->binary[0] = m->op + 2; // cmpf -1.0
        i->flags |= INST_FINALIZED;
      }
      else if(fabsf(operand - 1.0f) < FLT_EPSILON) {
        i->binary[0] = m->op + 1; // cmpf 1.0
        i->flags |= INST_FINALIZED;
      }
      else if(fabsf(operand) < FLT_EPSILON) {
        i->binary[0] = m->op; // cmpf 0.0
        i->flags |= INST_FINALIZED;
      }
      else {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand invalid for %s: %f is not in (-1.0, 0.0, 1.0)", &m->tag[0], operand);
      }
    }
    else {
      i->binary[0] = m->op + 3;
      i->flags |= INST_FINALIZED;
    }
  }
#endif
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }

  return result;
}


static int evmOptionalSerializer(const evm_mnemonic_t *m, evm_instruction_t *i) {
  int result = 0;

  i->binary[0] = m->op;
  i->count = 1;

  if(ARG_O1 <= m->arg && m->arg <= ARG_O8) {
    int32_t first = 0, second = 1;
    int count = sscanf(&i->text[0], "%*s %d %d", &first, &second);

    if(m->arg != ARG_I4_O4 || count >= 1) { // ensure the required args are given
      // validate the operand values
      if(m->arg == ARG_O3 && count >= 1 && (first < 1 || 8 < first)) {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (1 <= %d <= 8)", &m->tag[0], first);
      }
#if EVM_FLOAT_SUPPORT == 1
      else if(m->arg == ARG_O1 && count >= 1 && (first < 0 || 1 < first)) {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (0 <= %d <= 1)", &m->tag[0], first);
      }
#endif
      else if(m->arg == ARG_O4 && count >= 1 && (first < 1 || 16 < first)) {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (1 <= %d <= 16)", &m->tag[0], first);
      }
      else if(m->arg == ARG_I4_O4 && (first < 1 || 16 < first)) {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (1 <= %d <= 16)", &m->tag[0], first);
      }
      else if(m->arg == ARG_I4_O4 && count == 2 && (second < 1 || 16 < second)) {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (1 <= %d <= 16)", &m->tag[0], second);
      }
      else if(m->arg == ARG_O8 && count >= 1 && (first < 0 || 255 < first)) {
        result = -1;
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF("Operand out of bounds for %s (0 <= %d <= 255)", &m->tag[0], first);
      }
      // modify the opcode based on the operand
      else if(m->op == OP_POP_1) {
        i->binary[0] = m->op + (uint8_t) (first ? (first - 1) : 0);
        i->flags |= INST_FINALIZED;
      }
      else if(m->op == OP_REM_1) {
        if(second == 1 && first < 8) {
          i->binary[0] = m->op + (uint8_t) (first ? (first - 1) : 0);
          i->flags |= INST_FINALIZED;
        }
        else {
          i->binary[0] = OP_REM_R;
          i->binary[1] = (uint8_t) (((first - 1) << 4) | ((second - 1) & 0x0F));
          i->count++;
          i->flags |= INST_FINALIZED;
        }
      }
      else if(m->op == OP_DUP_0) {
        i->binary[0] = m->op + (uint8_t) (first ? (first - 1) : 0);
        i->flags |= INST_FINALIZED;
      }
      else if(m->op == OP_RET) {
        if(first < 15) {
          i->binary[0] = m->op + (uint8_t) first;
        }
        else {
          i->binary[0] = OP_RET_I;
          i->binary[1] = (uint8_t) first;
          i->count++;
        }
        i->flags |= INST_FINALIZED;
      }
#if EVM_FLOAT_SUPPORT == 1
      else if(m->op == OP_CONV_FI) {
        i->binary[0] = m->op + (uint8_t) first;
        i->flags |= INST_FINALIZED;
      }
      else if(m->op == OP_CONV_IF) {
        i->binary[0] = m->op + (uint8_t) first;
        i->flags |= INST_FINALIZED;
      }
#endif
      else {
        EVM_FATALF("Unhandled mnemonic encountered during optional operand processing: %s", &m->tag[0]);
      }
    }
    else {
      result = -1;
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }

  return result;
}

