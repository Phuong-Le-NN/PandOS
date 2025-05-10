/***********************deviceSupport.c***************************
 *  Description:
 *  This module provides support for user-level access to I/O
 *  devices including printers, terminals, disks, and flash memory.
 *  Helpers:
 *    - helper_check_string_outside_addr_space:
 *        Checks if a user string is outside legal memory bounds.
 *    - helper_copy_block:
 *        Copies a block of memory from source to destination.
 *  Functions includedd :
 *    - WRITE_TO_PRINTER(support_t*):
 *        Writes a string to the printer device, one character at a time.
 *        Returns success count or negative error code.
 *    - WRITE_TO_TERMINAL(support_t*):
 *        Writes a string to the terminal’s transmit register.
 *        Returns characters written or error code.
 *    - READ_FROM_TERMINAL(support_t*):
 *        Reads characters into a buffer from the terminal until newline.
 *        Returns character count or error code.
 *    - WRITE_TO_DISK(support_t*):
 *        Writes one block from a user buffer to disk at a given sector.
 *    - READ_FROM_DISK(support_t*):
 *        Reads one block from disk to a user buffer via DMA and block commands.
 *    - WRITE_TO_FLASH(support_t*):
 *        Writes a block to flash memory at a specified block number.
 *    - READ_FROM_FLASH(support_t*):
 *        Reads a block from flash memory into a user buffer.
 *  Written by Phuong Le 2025
 ***************************************************************/


#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../phase5/delayDaemon.h"

extern int masterSemaphore;
extern int mutex[DEVINTNUM * DEVPERINT + DEVPERINT];
extern int swapPoolSema4;
extern swapPoolFrame_t swapPoolTable[SWAP_POOL_SIZE];

/**********************************************************
 *  helper_check_string_outside_addr_space
 *
 *  Returns TRUE if the given string address falls outside
 *  the allowed logical address space for the user process.
 *
 *  Parameters:
 *         int strAdd – virtual address of the string
 *
 *  Returns:
 *         int – TRUE if address is invalid, FALSE otherwise
 **********************************************************/
HIDDEN int helper_check_string_outside_addr_space(int strAdd) {
	if((strAdd < KUSEG || strAdd > (LAST_USER_PAGE + PAGESIZE)) && (strAdd < UPROC_STACK_AREA || strAdd > (UPROC_STACK_AREA + PAGESIZE))) {
		return TRUE;
	}
	return FALSE;
}

/**********************************************************
 *  helper_copy_block
 *
 *  Copies a block of data from a source address to a destination address.
 *  The block is copied element by element in a loop, assuming the data is 
 *  4-byte aligned (i.e., the size of each element is 4 bytes).
 *
 *  Parameters:
 *  src
 *    Pointer to the source memory location from which data is copied.
 *    
 *  dst
 *    Pointer to the destination memory location where data is copied to.
 * 
 *  Returns
 *  
 **********************************************************/

HIDDEN void helper_copy_block(int *src, int *dst){
    int i;
    for (i = 0; i < (BLOCKSIZE/4); i++){
        *dst = *src;
        dst++; /*should increase by 4*/
        src++;
    }
}

/**********************************************************
 *  WRITE_TO_PRINTER
 *
 *  Writes a string to the printer device character by character.
 *  Handles errors for invalid addresses or invalid string length.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void WRITE_TO_PRINTER(support_t *passedUpSupportStruct) {
	/*
	virtual address of the first character of the string to be transmitted in a1,
	the length of this string in a2
	*/
	state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

	int devNo = passedUpSupportStruct->sup_asid - 1;
	device_t *printerDevAdd = devAddrBase(PRNTINT, devNo);

	/* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
	/* Error: length less than 0*/
	/* Error: a length greater than 128*/

	if(helper_check_string_outside_addr_space(savedExcState->s_a1) || (savedExcState->s_a2 < STR_MIN) || (savedExcState->s_a2 > STR_MAX)) {
		program_trap_handler(passedUpSupportStruct, NULL);
	}

	int mutexSemIdx = devSemIdx(PRNTINT, devNo, FALSE);
	SYSCALL(PASSERN, &(mutex[mutexSemIdx]), 0, 0);
	int i;
	int devStatus;
	for(i = 0; i < savedExcState->s_a2; i++) {
		setSTATUS(getSTATUS() & (~IECBITON));
		printerDevAdd->d_data0 = *(((char *)savedExcState->s_a1) + i); /*calculate address and accessing the current char*/
		printerDevAdd->d_command = PRINTCHR;
		devStatus = SYSCALL(IOWAIT, PRNTINT, devNo, 0); /*call SYSCALL IOWAIT to block until interrupt*/
		setSTATUS(getSTATUS() | IECBITON);
		if(devStatus != READY) { /* operation ends with a status other than "Device Ready" -- this is printer, not terminal */
			savedExcState->s_v0 = -devStatus;
			break;
		}
	}

	if(devStatus == READY) {
		savedExcState->s_v0 = i;
		;
	} else {
		savedExcState->s_v0 = -devStatus;
	}
	SYSCALL(VERHO, &(mutex[mutexSemIdx]), 0, 0);
}

/**********************************************************
 *  WRITE_TO_TERMINAL
 *
 *  Writes a string to the terminal device. Sends each character
 *  one at a time using the TRANSMITCHAR command.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void WRITE_TO_TERMINAL(support_t *passedUpSupportStruct) {
	/*
	virtual address of the first character of the string to be transmitted in a1,
	the length of this string in a2
	*/
	state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

	int devNo = passedUpSupportStruct->sup_asid - 1;
	device_t *termDevAdd = devAddrBase(TERMINT, devNo);

	/* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
	/* Error: length less than 0*/
	/* Error: a length greater than 128*/
	if(helper_check_string_outside_addr_space(savedExcState->s_a1) || (savedExcState->s_a2 < STR_MIN) || (savedExcState->s_a2 > STR_MAX)) {
		program_trap_handler(passedUpSupportStruct, NULL);
	}

	int mutexSemIdx = devSemIdx(TERMINT, devNo, FALSE);
	SYSCALL(PASSERN, &(mutex[mutexSemIdx]), 0, 0);
	int i;
	int transmStatus;
	for(i = 0; i < savedExcState->s_a2; i++) {
		setSTATUS(getSTATUS() & (~IECBITON));
		termDevAdd->t_transm_command = (*(((char *)savedExcState->s_a1) + i) << TRANS_COMMAND_SHIFT) + TRANSMIT_COMMAND;
		transmStatus = SYSCALL(IOWAIT, TERMINT, devNo, FALSE); /*call SYSCALL IOWAIT to block until interrupt*/
		setSTATUS(getSTATUS() | IECBITON);
		if((transmStatus & STATUS_CHAR_MASK) != CHAR_TRANSMITTED) { /* operation ends with a status other than Character Transmitted */
			savedExcState->s_v0 = -transmStatus;
			break;
		}
	}

	if((transmStatus & STATUS_CHAR_MASK) == CHAR_TRANSMITTED) {
		savedExcState->s_v0 = i;
		;
	} else {
		savedExcState->s_v0 = -transmStatus;
	}
	SYSCALL(VERHO, &(mutex[mutexSemIdx]), 0, 0);
}

/**********************************************************
 *  READ_FROM_TERMINAL
 *
 *  Reads characters from the terminal input device until newline.
 *  Stores characters in the buffer provided by the user process.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void READ_FROM_TERMINAL(support_t *passedUpSupportStruct) {
	/* Get the terminal device register */
	state_t *savedExcState = &(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]);

	int devNo = passedUpSupportStruct->sup_asid - 1;
	device_t *termDevAdd = devAddrBase(TERMINT, devNo);

	/* Error: to write to a printer device from an address outside of the requesting U-proc’s logical address space*/
	/* Error: length less than 0*/
	/* Error: a length greater than 128*/
	if(helper_check_string_outside_addr_space(savedExcState->s_a1) || (savedExcState->s_a2 < STR_MIN) || (savedExcState->s_a2 > STR_MAX)) {
		program_trap_handler(passedUpSupportStruct, NULL);
	}

	int mutexSemIdx = devSemIdx(TERMINT, devNo, TRUE);
	SYSCALL(PASSERN, &(mutex[mutexSemIdx]), 0, 0);

	char *stringAdd = savedExcState->s_a1;

	int i = 0;
	int recvStatusField;
	int recvStatus;
	char recvChar = 'a';
	while(recvChar != NEW_LINE) {
		setSTATUS(getSTATUS() & (~IECBITON));
		termDevAdd->t_recv_command = RECEIVE_COMMAND;
		recvStatusField = SYSCALL(IOWAIT, TERMINT, devNo, TRUE); /*call SYSCALL IOWAIT to block until interrupt*/
		setSTATUS(getSTATUS() | IECBITON);
		recvChar = (recvStatusField & RECEIVE_CHAR_MASK) >> RECEIVE_COMMAND_SHIFT;
		recvStatus = recvStatusField & STATUS_CHAR_MASK;
		stringAdd[i] = recvChar; /* write the char into the string buffer array */
		i++;
		if(recvStatus != CHAR_RECIEVED) { 
            /* operation ends with a status other than Character Received */
			savedExcState->s_v0 = -recvStatus;
			break;
		}
	}

	if(recvStatus == CHAR_RECIEVED) {
		savedExcState->s_v0 = i;
	} else {
		savedExcState->s_v0 = -recvStatus;
	}
	SYSCALL(VERHO, &(mutex[mutexSemIdx]), 0, 0);
}

/**********************************************************
 *  WRITE_TO_DISK
 *
 *  Writes a block of data to the disk. It checks if the address 
 *  is within a valid range, prepares the disk for writing, and 
 *  performs the disk write operation.
 *
 *  Parameters:
 *         support_t *currentSupport – pointer to the support struct
 *
 *  Returns:
 *         None
 *
 **********************************************************/
void WRITE_TO_DISK(support_t *currentSupport){
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);

    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    /*get disk capacity*/
    int maxcyl = ((disk_dev_reg_addr->d_data1) >> MAXCYL_SHIFT) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> MAXHEAD_SHIFT) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    /*when setting up backing store as disk, depending on where on disk storing an image, it should be illegal to write there too*/
    if ((saved_gen_exc_state->s_a1 < KUSEG) || (saved_gen_exc_state->s_a3 > (maxcyl*maxhead*maxsect))){
        program_trap_handler(currentSupport, NULL);
    }

    /* start the operation with mutual exclusion */
    SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
        /*calcutate the sect, head, cyl for the command*/
        int sectNo = (saved_gen_exc_state->s_a3) % maxsect;
	    int headNo = ((int) ((saved_gen_exc_state->s_a3) / (maxsect * maxcyl))) % maxhead; /*divide and round down*/
        int cylNo = ((int) ((saved_gen_exc_state->s_a3) / maxsect)) % maxcyl;
        /*seek to get head to the right position*/
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL; /*seek*/
            int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            saved_gen_exc_state->s_v0 = 0 - disk_status;
            SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
            return;
        }
        /*bring content into buffer and start the command to write from buffer*/
        helper_copy_block(saved_gen_exc_state->s_a1, DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo));
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo); 
        /*carefull not to do anything with device between writing to data0 and command*/
        /*start operation with command and syscall atomically*/
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + WRITEBLK_DSK; /*write*/
            disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);

    if (disk_status == READY){
        saved_gen_exc_state->s_v0 = disk_status;
    } else {
        saved_gen_exc_state->s_v0 = 0 - disk_status;
    }
}

/**********************************************************
 *  READ_FROM_DISK
 *
 *  Reads a block of data from the disk. It checks if the address 
 *  is within a valid range, prepares the disk for reading, and 
 *  performs the disk read operation.
 *
 *  Parameters:
 *         support_t *currentSupport – pointer to the support struct
 *
 *  Returns:
 *         None
 *
 **********************************************************/
void READ_FROM_DISK(support_t *currentSupport){
    state_PTR saved_gen_exc_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);

    int devNo = saved_gen_exc_state->s_a2;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    /*get disk capacity*/
    int maxcyl = ((disk_dev_reg_addr->d_data1) >> MAXCYL_SHIFT) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> MAXHEAD_SHIFT) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    /*when setting up backing store as disk, depending on where on disk storing an image, it should be illegal to write there too*/
    if ((saved_gen_exc_state->s_a1 < KUSEG) || (saved_gen_exc_state->s_a3 > (maxcyl*maxhead*maxsect))){
        program_trap_handler(currentSupport, NULL);
    }
    /* start the operation with mutual exclusion */
    SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
        int sectNo = saved_gen_exc_state->s_a3 % maxsect;
        int headNo = ((int) (saved_gen_exc_state->s_a3 / (maxsect * maxcyl))) % maxhead;
        int cylNo = ((int) (saved_gen_exc_state->s_a3 / maxsect)) % maxcyl;
        /*seek to get head to the right head position*/
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL;
            int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            saved_gen_exc_state->s_v0 = 0 - disk_status;
            SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
            return;
        }
        /*read into buffer*/
        disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo);
        /*start operation with command and syscall atomically*/
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + READBLK_DSK;
            disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        /*bring content into buffer after read to buffer*/
        helper_copy_block(DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo), saved_gen_exc_state->s_a1);
    SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);

    if (disk_status == READY){
        saved_gen_exc_state->s_v0 = disk_status;
    } else{
        saved_gen_exc_state->s_v0 = 0 - disk_status;
    }
}
/**********************************************************
 *  READ_FROM_FLASH
 *
 *  Reads a block of data from the flash memory. It checks if the address 
 *  is within a valid range, prepares the flash for reading, and 
 *  performs the flash read operation.
 *
 *  Parameters:
 *         support_t *currentSupport – pointer to the support struct
 *
 *  Returns:
 *         None
 *
 **********************************************************/
void READ_FROM_FLASH(support_t *currentSupport){
    state_PTR saved_exception_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_exception_state->s_a2;
    int flash_sem_idx = devSemIdx(FLASHINT, devNo, FALSE);

    device_t *flash_dev_reg_addr = devAddrBase(FLASHINT, devNo);

    /* should be illegal to read from backing store area in flash and outside logical address space and flash capacity*/
    if ((saved_exception_state->s_a1 < KUSEG) || (saved_exception_state->s_a3 < FIRST_BLOCK_NEXT_BSK) || (saved_exception_state->s_a3 >= flash_dev_reg_addr->d_data1)){ 
        program_trap_handler(currentSupport, NULL);
    }
    /* start reading into buffer with mutex */
    SYSCALL(PASSERN, &(mutex[flash_sem_idx]), 0, 0);
    /*start operation with command and syscall atomically*/
        flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo;
        setSTATUS(getSTATUS() & (~IECBITON));
            flash_dev_reg_addr->d_command = (saved_exception_state->s_a3 << BLOCKNUM_SHIFT) + READBLK_FLASH;
            int flash_status = SYSCALL(IOWAIT,FLASHINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        /* copy from buffer to desired memory location*/
        helper_copy_block(FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo, saved_exception_state->s_a1);
    SYSCALL(VERHO, &(mutex[flash_sem_idx]), 0, 0);

    if (flash_status == READY){
        saved_exception_state->s_v0 = flash_status;
    } else{
        saved_exception_state->s_v0 = 0 - flash_status;
    }
}

/**********************************************************
 *  WRITE_TO_FLASH
 *
 *  Writes a block of data to the flash memory. It checks if the address 
 *  is within a valid range, prepares the flash for writing, and 
 *  performs the flash write operation.
 *
 *  Parameters:
 *         support_t *currentSupport – pointer to the support struct
 *
 *  Returns:
 *         None
 *
 **********************************************************/
void WRITE_TO_FLASH(support_t *currentSupport){
    state_PTR saved_exception_state = &(currentSupport->sup_exceptState[GENERALEXCEPT]);
    int devNo = saved_exception_state->s_a2;
    int flash_sem_idx = devSemIdx(FLASHINT, devNo, FALSE);
    
    device_t *flash_dev_reg_addr = devAddrBase(FLASHINT, devNo);

    /* should be illegal to read from backing store area in flash and outside logical address space and flash capacity*/
    if ((saved_exception_state->s_a1 < KUSEG) || (saved_exception_state->s_a3 < FIRST_BLOCK_NEXT_BSK) || (saved_exception_state->s_a3 >= flash_dev_reg_addr->d_data1)){
        program_trap_handler(currentSupport, NULL);
    }
    
    /* start writing from buffer with mutex */
    SYSCALL(PASSERN, &(mutex[flash_sem_idx]), 0, 0);
        /* copy into buffer from desired memory location*/
        helper_copy_block(saved_exception_state->s_a1, FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo);
        /*start operation with command and syscall atomically*/
        flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + BLOCKSIZE*devNo;
        setSTATUS(getSTATUS() & (~IECBITON));
            flash_dev_reg_addr->d_command = (saved_exception_state->s_a3 << BLOCKNUM_SHIFT) + WRITEBLK_FLASH;
            int flash_status = SYSCALL(IOWAIT, FLASHINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[flash_sem_idx]), 0, 0);

    if (flash_status == READY){
        saved_exception_state->s_v0 = flash_status;
    } else{
        saved_exception_state->s_v0 = 0 - flash_status;
    }
}
