; simple example assembly file for eVM
.name MAIN
.offset 0

  JMP entry

function:
  BLTIN 0 ; generate program checksum
  BLTIN 1 ; dump stack to stdout
  BLTIN 2 ; dump the program to stdout
  RET   1 ; use the return address 1 down from the top of the stack

entry:
  CALL function
  POP
  HALT

