
/* concatenates two strings together and prints them out */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

void main() {
	char buf[20];
	char *welcome = "IT IS SPRING HERE ARE SOME FLOWER FOR YOU\n";

	int leng = 0;
	for(leng = 0; welcome[leng] != '\0'; leng++)
		;

	SYSCALL(WRITETERMINAL, (int)welcome, leng, 0);

	int i;
	int j;
	for(i = 0; i < 10; i++) {
		for(j = 0; j < i; j++) {
			buf[j] = '_';
		}
		buf[j] = '*';
		buf[j + 1] = 0x0A;
		SYSCALL(WRITETERMINAL, (int)&buf[0], j + 2, 0);
	}

	/* Terminate normally */
	SYSCALL(TERMINATE, 0, 0, 0);
}
