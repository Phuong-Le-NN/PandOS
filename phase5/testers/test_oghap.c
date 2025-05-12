/* Simple test that asks user input to check printer and terminal is working */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main() {
	int status, i;
	char buf[100];
	char buf2[100];

	print(WRITETERMINAL, "Simple Printer Test starts\n");
	print(WRITETERMINAL, "Write something to print: ");

	status = SYSCALL(READTERMINAL, (int)&buf[0], 0, 0);

	if(status > 100) {
		SYSCALL(TERMINATE, 0, 0, 0);
	}

	buf[status] = EOS;

	print(WRITETERMINAL, "\n");

	i = 0;
	for(i = 0; i < status - 1; i++) {
		buf2[i] = buf[i];
	}

	buf2[status] = EOS;

	print(WRITETERMINAL, &buf2[0]);

	print(WRITETERMINAL, "\n\nInput String Printed \n");

	SYSCALL(TERMINATE, 0, 0, 0);
}
