/*	Test of Delay and Get Time of Day */

#include "../../h/localLibumps.h"
#include "../../h/print.h"
#define FALSE			0
#define TRUE			1
#define PAGESIZE		4096
#define SECOND			1000000
#define EOS				'\0'
#define PAGESIZE		4096
#define WORDLEN			4
#define READY			1


#define	GETTIME			6

/* level 1 SYS calls */
#define TERMINATE		9
#define GET_TOD			10
#define WRITEPRINTER	11
#define WRITETERMINAL 	12
#define READTERMINAL	13
#define DISK_PUT		14
#define DISK_GET		15
#define FLASH_PUT		16
#define	FLASH_GET		17
#define DELAY			18
#define PSEMVIRT		19
#define VSEMVIRT		20

#define SEG0			0x00000000
#define SEG1			0x40000000
#define SEG2			0x80000000
#define SEG3			0xC0000000

void main() {
	unsigned int now1 ,now2;

	now1 = SYSCALL(GET_TOD, 0, 0, 0);
	print(WRITETERMINAL, "Delay test starts\n");
	now1 = SYSCALL(GET_TOD, 0, 0, 0);

	now2 = SYSCALL(GET_TOD, 0, 0, 0);

	if (now2 < now1)
		print(WRITETERMINAL, "Delay Test error: time decreasing\n");
	else
		print(WRITETERMINAL, "Delay Test ok: time increasing\n");

	SYSCALL(DELAY, 2, 0, 0);		/* Delay 2 second */
	now1 = SYSCALL(GET_TOD, 0, 0, 0);

	if ((now1 - now2) < SECOND)
		print(WRITETERMINAL, "Delay Test error: did not delay one second\n");
	else
		print(WRITETERMINAL, "Delay Test ok: two second delay\n");
		
	print(WRITETERMINAL, "Delay Test completed\n");
		
	/* Try to execute nucleys system call. Should cause termination */
	now1 = SYSCALL(GETTIME, 0, 0, 0);
	
	print(WRITETERMINAL, "todTest error: SYS6 did not terminate\n");
	SYSCALL(TERMINATE, 0, 0, 0);
}
