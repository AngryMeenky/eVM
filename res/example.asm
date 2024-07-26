; simple example assembly file for eVM
.name MAIN
.offset 0

  JMP entry

function:
  BLTIN 0 ; generate program checksum
  BLTIN 1 ; dump stack to stdout
  BLTIN 2 ; dump the program to stdout
  RET

entry:
  CALL function
  HALT

