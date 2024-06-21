#include "evm/evm_asm.h"
#include "evm/opcodes.h"

#include <ctype.h>
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
  INST_FINALIZED   = 1 << 4,
  INST_UNRESOLVED  = 1 << 5,
  INST_INVALID_ARG = 1 << 6,
  INST_MISSING_ARG = 1 << 7,
} evm_flags_t;


struct evm_mnemonic_s;
typedef struct evm_mnemonic_s evm_mnemonic_t;


typedef void (*evm_serializer_t)(const struct evm_mnemonic_s *, evm_instruction_t *);


struct evm_mnemonic_s {
  const char       *tag;
  evm_arg_t         arg;
  opcode_t          op;
  evm_serializer_t  process;
};


typedef struct evm_label_s {
  struct evm_label_s *next;
  struct evm_label_s *prev;
  uint32_t            offset;
  uint32_t            id;
  char                name[];
} evm_label_t;


typedef struct evm_section_s {
  struct evm_section_s *next;
  struct evm_section_s *prev;
  uint8_t              *contents;
  evm_label_t           labels;
  uint32_t              base;
  uint32_t              length;
  uint32_t              capacity;
  char                  name[];
} evm_section_t;


struct evm_program_s {
  evm_section_t sections;
  uint32_t      base;
  uint32_t      length;
  uint32_t      count;
};


static void               evmasmClearLabelList(evm_label_t *);
static void               evmasmClearSectionList(evm_section_t *);
static void               evmasmClearInstructionList(evm_instruction_t *);
static void               evmasmAppendInstruction(evm_instruction_t *, evm_instruction_t *);
static evm_instruction_t *evmasmNewInstruction(const char *, const char *, uint32_t);
static evm_program_t     *evmasmNewProgram();
static void               evmasmDeleteProgram(evm_program_t *);
static evm_section_t     *evmasmNewSection(const char *);
static void               evmasmDeleteSection(evm_section_t *);
static evm_label_t       *evmasmNewLabel(const char *, uint32_t);
static int                mnemonicCompare(const char *, const char *, const char *);


evm_assembler_t *evmasmAllocate() {
  return (evm_assembler_t *) calloc(1, sizeof(evm_assembler_t));
}


evm_assembler_t *evmasmInitialize(evm_assembler_t *evm) {
  if(evm) {
    memset((void *) evm, 0, sizeof(evm_assembler_t));
    evm->head.prev = &evm->head;
    evm->head.next = &evm->head;
  }

  return evm;
}


evm_assembler_t *evmasmFinalize(evm_assembler_t *evm) {
  if(evm) {
    evmasmClearInstructionList(&evm->head);
    memset((void *) evm, 0, sizeof(evm_assembler_t));
  }

  return evm;
}


void evmasmFree(evm_assembler_t *evm) {
  free(evm);
}


int evmasmParseFile(evm_assembler_t *evm, FILE *fp) {
  int result = 0;

  if(evm && fp) {
    int line = 0;
    char buffer[BUFSIZ];

    while(fgets(&buffer[0], BUFSIZ, fp)) {
      result |= evmasmParseLine(evm, &buffer[0], ++line);
    }
  }
  else {
    result = -1;
  }

  return result;
}


static void evmPushSerializer(const struct evm_mnemonic_s *, evm_instruction_t *);
static void evmLabelSerializer(const struct evm_mnemonic_s *, evm_instruction_t *);
static void evmSimpleSerializer(const struct evm_mnemonic_s *, evm_instruction_t *);
static void evmCompareSerializer(const struct evm_mnemonic_s *, evm_instruction_t *);
static void evmOptionalSerializer(const struct evm_mnemonic_s *, evm_instruction_t *);


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
  // OP_DUP_{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}
  { "DUP",   ARG_O4,    OP_DUP_1,   &evmOptionalSerializer },
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
  { "JMP",   ARG_LBL,   OP_JMP,     &evmLabelSerializer    }, // OP_JMP,OP_LJMP
  { "JLT",   ARG_LBL,   OP_JLT,     &evmLabelSerializer    }, // OP_JLT,OP_LJLT
  { "JLE",   ARG_LBL,   OP_JLE,     &evmLabelSerializer    }, // OP_JLE,OP_LJLE
  { "JNE",   ARG_LBL,   OP_JNE,     &evmLabelSerializer    }, // OP_JNE,OP_LJNE
  { "JEQ",   ARG_LBL,   OP_JEQ,     &evmLabelSerializer    }, // OP_JEQ,OP_LJEQ
  { "JGE",   ARG_LBL,   OP_JGE,     &evmLabelSerializer    }, // OP_JGE,OP_LJGE
  { "JGT",   ARG_LBL,   OP_JGT,     &evmLabelSerializer    }, // OP_JGT,OP_LJGT
  { "JTBL",  ARG_NONE,  OP_JTBL,    &evmSimpleSerializer   }, // OP_JTBL,OP_LJTBL
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


int evmasmParseLine(evm_assembler_t *evm, const char *line, int num) {
  const char *start = line;
  const char *end = &line[strlen(line)];
  const char *cur;
  int result = -1;

  // trim leading whitespace
  while(start != end && isspace(*start)) { ++start; }
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
    if(*start == '.') {
      // process directives
      
    }
    else if(end[-1] == ':') {
      // process labels
    }
    else {
      const evm_mnemonic_t *mnemonic;

      // parse instructions
      for(mnemonic = &MNEMONICS[0]; mnemonic->tag; ++mnemonic) {
        if(!mnemonicCompare(&mnemonic->tag[0], start, end)) {
          evm_instruction_t *inst = evmasmNewInstruction(start, end, num);
          evmasmAppendInstruction(&evm->head, inst);
          mnemonic->process(mnemonic, inst);
          break;
        }
      }
    }
  }
  else {
    result = 0; 
  }

  return result;
}


int evmasmValidateProgram(evm_assembler_t *evm) {
  // TODO: ensure no duplicate labels
  // TODO: ensure all jmp targets are resolved
  // TODO: ensure that no sections overlap
  // TODO: ensure that all sections contain data or instructions
  // TODO: ensure first byte is a valid instruction and not data
  return -1;
}


uint32_t evmasmProgramSize(const evm_assembler_t *evm) {
  if(evm) {
    return evm->length;
  }

  return 0;
}


uint32_t evmasmProgramToBuffer(const evm_assembler_t *evm, uint8_t *buf, uint32_t max) {
  if(evm && buf) {
    if(evm->output || !evmasmValidateProgram((evm_assembler_t *) evm)) {
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
  if(evm && fp) {
    int result = 0;
    if(evm->output || !evmasmValidateProgram((evm_assembler_t *) evm)) {
      uint8_t *buffer = malloc(evm->length);

      if(buffer) {
        if(!evmasmProgramToBuffer(evm, buffer, evm->length)) {
          result = fwrite(buffer, 1, evm->length, fp) == evm->length ? 0 : -1;
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
    else {
      result = -1;
    }

    return result;
  }

  return -1;
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


static evm_instruction_t *evmasmNewInstruction(const char *start, const char *end, uint32_t line) {
  evm_instruction_t *inst = calloc(1, (end - start) + sizeof(evm_instruction_t) + 1U);
  if(inst) {
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
  }

  return prog;
}


static void evmasmDeleteProgram(evm_program_t *prog) {
  if(prog) {
    evmasmClearSectionList(&prog->sections);
    free(prog);
  }
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


static evm_label_t *evmasmNewLabel(const char *name, uint32_t id) {
  evm_label_t *label = calloc(1, sizeof(evm_label_t) + strlen(name) + 1U);

  if(label) {
    label->offset = 0xFF000000U; // maximum section size is 24bits
    label->id = id;
    strcpy(&label->name[0], name);
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
    else if(start != end && (!isspace(*start) || *start != ';')) {
      result = -1; // mnemonic is longer than the tag
    }
  }

  return result;
}


static void evmPushSerializer(const struct evm_mnemonic_s *m, evm_instruction_t *i) {
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
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
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
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }
}


static void evmLabelSerializer(const struct evm_mnemonic_s *m, evm_instruction_t *i) {
  i->binary[0] = m->op;
  i->count = 1;

  if(m->arg == ARG_LBL) {
    const char *ptr = &i->text[0];
    // skip the mnemonic
    while(*ptr && !isspace(*ptr)) { ++ptr; }
    // skip the whitespace
    while(*ptr && isspace(*ptr)) { ++ptr; }

    if(*ptr) {
      i->binary[i->count] = (int8_t) (ptr - &i->text[0]);
      i->flags |= INST_UNRESOLVED;
    }
    else {
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }
}


static void evmSimpleSerializer(const struct evm_mnemonic_s *m, evm_instruction_t *i) {
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
        i->flags |= INST_INVALID_ARG;
        EVM_ERRORF( "Operand out of bounds for %s (-128 <= %d <= 127)", &m->tag[0], operand);
      }
    }
    else {
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }
}


static void evmCompareSerializer(const struct evm_mnemonic_s *m, evm_instruction_t *i) {
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
        i->binary[0] = m->op + 3;
        i->flags |= INST_FINALIZED;
      }
    }
    else {
      i->flags |= INST_MISSING_ARG;
      EVM_ERRORF("Missing operand for %s", &m->tag[0]);
    }
  }
  else {
    EVM_FATALF("Unsupported operand type while processing %s", &m->tag[0]);
  }
}


static void evmOptionalSerializer(const struct evm_mnemonic_s *m, evm_instruction_t *i) {
}

