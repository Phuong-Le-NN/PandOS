/*	Test Flash Get and Flash Put */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

#define DISK_PUT 14
#define DISK_GET 15
#define FLASH_PUT 16
#define FLASH_GET 17
#define MILLION	1000000

void main() {
	/*char buffer[PAGESIZE]; */
	int i;
	int dstatus;
	char *buffer;
	
	buffer = (char *)(SEG2 + (20 * PAGESIZE));

	print(WRITETERMINAL, "flashTest starts\n");
	*buffer = 'a';  /*buffer[0] = 'a'; */
	dstatus = SYSCALL(FLASH_PUT, (int)buffer, 1, 34);
	
	if (dstatus != READY)
		print(WRITETERMINAL, "flashTest error: flash i/o result\n");
	else
		print(WRITETERMINAL, "flashTest ok: flash i/o result\n");

	*buffer = 'b';  /*buffer[0] = 'z'; */
	dstatus = SYSCALL(FLASH_PUT, (int)buffer, 1, 35);

	dstatus = SYSCALL(FLASH_GET, (int)buffer, 1, 34);
	
	if (*buffer != 'a')  /*(buffer[0] != 'a') */
		print(WRITETERMINAL, "flashTest error: bad first flash sector readback\n");
	else
		print(WRITETERMINAL, "flashTest ok: first flash sector readback\n");

	dstatus = SYSCALL(FLASH_GET, (int)buffer, 1, 35);
	
	if (*buffer != 'b') /*(buffer[0] != 'z') */
		print(WRITETERMINAL, "flashTest error: bad second flash sector readback\n");
	else
		print(WRITETERMINAL, "flashTest ok: second flash sector readback\n");


    print(WRITETERMINAL, "diskTest starts\n");
	*buffer = 'a';  /*buffer[0] = 'a'; */
	dstatus = SYSCALL(DISK_PUT, (int)buffer, 1, 3);
	
	if (dstatus != READY)
		print(WRITETERMINAL, "diskTest error: disk i/o result\n");
	else
		print(WRITETERMINAL, "diskTest ok: disk i/o result\n");

	*buffer = 'b';  /*buffer[0] = 'z'; */
	dstatus = SYSCALL(DISK_PUT, (int)buffer, 1, 23);

	dstatus = SYSCALL(DISK_GET, (int)buffer, 1, 3);
	
	if (*buffer != 'a')  /*(buffer[0] != 'a') */
		print(WRITETERMINAL, "diskTest error: bad first disk sector readback\n");
	else
		print(WRITETERMINAL, "diskTest ok: first disk sector readback\n");

	dstatus = SYSCALL(DISK_GET, (int)buffer, 1, 23);
	
	if (*buffer != 'b') /*(buffer[0] != 'z') */
		print(WRITETERMINAL, "diskTest error: bad second disk sector readback\n");
	else
		print(WRITETERMINAL, "diskTest ok: second disk sector readback\n");

	
	SYSCALL(TERMINATE, 0, 0, 0);
}