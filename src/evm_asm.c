#include "evm/evm_asm.h"
#include "evm/opcodes.h"

#include <stdlib.h>


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
} evm_section_t;


struct evm_program_s {
  evm_section_t sections;
  uint32_t      base;
  uint32_t      length;
  uint32_t      count;
};


static void evmasmClearLabelList(evm_label_t *);
static void evmasmClearSectionList(evm_section_t *);
static void evmasmClearInstructionList(evm_instruction_t *);


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


int evmasmParseLine(evm_assembler_t *evm, const char *line, int num) {
  // TODO: remove comments
  // TODO: process directives
  // TODO: parse instructions
  return -1;
}


int evmasmValidateProgram(evm_assembler_t *evm) {
  // TODO: ensure no duplicate labels
  // TODO: ensure all jmp targets are resolved
  // TODO: ensure that now sections overlap
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
        memset(buf, 0, evm->length);
        // TODO: convert program to flat buffer
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


static void evmasmClearSectionList(evm_section_t *list) {
  evm_section_t *node, *tmp;

  for(node = list->next; node != list; node = tmp) {
    tmp = node->next;
    free(node);
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

