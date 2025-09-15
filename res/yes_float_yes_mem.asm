; simple example assembly file for eVM
.name MAIN
.offset 0

entry:
  NOP
  PUSH 100000000 ; five byte push
  DUP            ; duplicate the top value in the stack
  ADD            ; pop the top two values and push the result of addition
  PUSH 0         ; single byte push
  SUB            ; pop the top two values and push the result of subtraction
  ABS            ; replace the top of the stack with its absolute value
  PUSH 8192      ; three byte push
  SWAP           ; swap the top two stack values
  PUSH -1        ; single byte push
  PUSH 63        ; two byte push
  MUL            ; pop the top two values and push the result of multiplication
  PUSH 1         ; single byte push
  SWAP           ; swap the top two stack values
  DIV            ; pop the top two values and push the result of division
  NEG            ; replace the top of the stack with its negated value
  PUSH 86400     ; four byte push
  INC            ; increment the top of the stack
  DEC            ; decrement the top of the stack
  CALL report
  POP 5          ; empty the stack
  PUSHF 0.0      ; single byte push
  PUSHF 1.0      ; single byte push
  ADDF           ; pop the top two values and push the result of addition
  DUP
  INCF           ; increment the top of the stack
  DIVF           ; pop the top two values and push the result of division
  PUSHF -1.0     ; single byte push
  SUBF           ; pop the top two values and push the result of subtraction
  PUSHF 10.0     ; five byte push
  MULF           ; pop the top two values and push the result of multiplication
  DECF           ; decrement the top of the stack
  NEGF           ; replace the top of the stack with its negated value
  ABSF           ; replace the top of the stack with its absolute value
  CNVFI          ; convert the top of the stack from float to int
  BLTIN 1        ; dump stack to stdout
  POP            ; empty the stack again
  HALT

unreachable:
  CNVFI 1        ; convert the second value in the stack from float to int
  CNVIF          ; convert the top of the stack from int to float
  CNVIF 1        ; convert the second value in the stack from int to float
  DUP  2         ; duplicate the second value in the stack
  DUP  3         ; duplicate the third value in the stack
  DUP  4         ; duplicate the fourth value in the stack
  DUP  5         ; duplicate the fifth value in the stack
  DUP  6         ; duplicate the sixth value in the stack
  DUP  7         ; duplicate the seventh value in the stack
  DUP  8         ; duplicate the eighth value in the stack
  DUP  9         ; duplicate the ninth value in the stack
  DUP 10         ; duplicate the tenth value in the stack
  DUP 11         ; duplicate the eleventh value in the stack
  DUP 12         ; duplicate the twelfth value in the stack
  DUP 13         ; duplicate the thirteenth value in the stack
  DUP 14         ; duplicate the fourteenth value in the stack
  DUP 15         ; duplicate the fifteenth value in the stack
  DUP 16         ; duplicate the sixteenth value in the stack
  TRUNC 8       ; zero out the high three bytes on the top value of the stack
  SIGNEXT 8     ; convert a signed byte to a signed int
  SEG 255       ; set the current memory segment
  READ    0x1000
  WRITE8  0x2000
  WRITE16 0x2002
  WRITE24 0x2004
  WRITE32 0x2008
  LREAD    0xFF1000
  LWRITE8  0xFF2000
  LWRITE16 0xFF2002
  LWRITE24 0xFF2004
  LWRITE32 0xFF2008
  SREAD
  SWRITE8
  SWRITE16
  SWRITE24
  SWRITE32
  JTBL
.addr unreachable
.addr report
.addr entry
.addr returns
  POP            ; pop the top value from the stack
  POP 2          ; pop the top two values from the stack
  POP 3          ; pop the top three values from the stack
  POP 4          ; pop the top four values from the stack
  POP 5          ; pop the top five values from the stack
  POP 6          ; pop the top six values from the stack
  POP 7          ; pop the top seven values from the stack
  POP 8          ; pop the top eight values from the stack
  LJTBL
.addr unreachable
.addr report
.addr entry
.addr returns
  REM 1          ; remove the second value from the stack
  REM 2          ; remove the third value from the stack
  REM 3          ; remove the fourth value from the stack
  REM 4          ; remove the fifth value from the stack
  REM 5          ; remove the sixth value from the stack
  REM 6          ; remove the seventh value from the stack
  REM 7          ; remove the eighth value from the stack
  REM 12 3       
  LSH
  RSH
  AND
  OR
  XOR
  INV
  BOOL
  NOT
  CMP -1
  CMP  0
  CMP  1
  CMP
  CMPF -1.0
  CMPF  0.0
  CMPF  1.0
  CMPF
  JMP unreachable
  JLT unreachable
  JLE unreachable
  JNE unreachable
  JEQ unreachable
  JGE unreachable
  JGT unreachable
  LJMP unreachable
  LJLT unreachable
  LJLE unreachable
  LJNE unreachable
  LJEQ unreachable
  LJGE unreachable
  LJGT unreachable

returns:
  RET
  RET  1
  RET  2
  RET  3
  RET  4
  RET  5
  RET  6
  RET  7
  RET  8
  RET  9
  RET 10
  RET 11
  RET 12
  RET 13
  RET 14
  RET 144
  

report:
  BLTIN 0 ; generate program checksum and push it onto the stack
  BLTIN 1 ; dump stack to stdout
  BLTIN 2 ; dump the program to stdout
  RET   1 ; use the return address 1 down from the top of the stack


