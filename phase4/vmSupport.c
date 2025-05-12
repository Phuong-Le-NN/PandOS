/*********************************VMSUPPORT.C*******************************
 *
 *  Virtual Memory Support Module
 *
 *  This module implements the TLB exception handler which handles page faults
 *  for U-procs. When a page is not in memory, the Pager loads it from secondary
 *  storage (flash device).
 *
 *  Because U-procs can only access flash devices for paging purposes, this
 *  module also includes functions to handle read and write operations
 *  between memory and flash storage.
 *
 *  Additionally, this module maintains:
 *  - A swap pool table that tracks which physical frames are currently in use
 *  - A swap pool semaphore used to ensure synchronized access to the swap pool
 *
 *
 *      Modified by Phuong and Oghap on March 2025
 */

#include "vmSupport.h"

/**********************************************************
 *  initSwapStruct
 *
 *  Initializes the swap pool table and the swap pool semaphore.
 *  Sets all swap pool entries to unused state.
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 **********************************************************/
void initSwapStruct() {
	/* initialize the swap pool structure */
	int i;
	for(i = 0; i < SWAP_POOL_SIZE; i++) {
		swapPoolTable[i].ASID = -1;
		swapPoolTable[i].pgNo = -1;
		swapPoolTable[i].matchingPgTableEntry = NULL;
	}
	swapPoolSema4 = 1;
}

/**********************************************************
 *  uTLB_RefillHandler
 *
 *  Handles TLB refill exceptions by inserting the missing
 *  page’s mapping into the TLB from the current process's page table.
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 **********************************************************/
void uTLB_RefillHandler() {
	int missingVPN = (((state_PTR)BIOSDATAPAGE)->s_entryHI >> VPN_SHIFT) & VPN_MASK;

	/* Get the Page Table entry for page number p for the Current Process. This will be located in the Current Process’s Page Table*/
	int missingVPN_idx_in_pgTable = missingVPN % PAGE_TABLE_SIZE;

	support_t *currentSupport = currentP->p_supportStruct;
	pte_t *pte = &(currentSupport->sup_privatePgTbl[missingVPN_idx_in_pgTable]);

	/* Write this Page Table entry into the TLB*/
	setENTRYHI(pte->EntryHi);
	setENTRYLO(pte->EntryLo);
	TLBWR();

	LDST((state_PTR)BIOSDATAPAGE);
}

/**********************************************************
 *  page_replace
 *
 *  Selects a free or replaceable frame from the swap pool
 *  using a simple FIFO algorithm.
 *
 *  Parameters:
 *
 *
 *  Returns:
 *         int – index of the selected swap pool frame
 **********************************************************/
int page_replace() {
	static int nextFrame = 0;

	/* Look for an empty frame */
	int pickedFrame;
	for(pickedFrame = 0; pickedFrame < SWAP_POOL_SIZE; pickedFrame = pickedFrame + 1) {
		if(swapPoolTable[pickedFrame].ASID == -1) {
			/* so that frame i doesn't get replace right away next time but only after cirulated */
			if(pickedFrame == nextFrame) {
				nextFrame = (nextFrame + 1) % SWAP_POOL_SIZE;
			}
			return pickedFrame;
		}
	}

	/* If no free frame, select the oldest one (FIFO) */
	int selectedFrame = nextFrame;
	/* Move to next in circular order */
	nextFrame = (nextFrame + 1) % (SWAP_POOL_SIZE);

	return selectedFrame;
}

/**********************************************************
 *  read_write_flash
 *
 *  Performs a flash read or write operation for paging.
 *  Transfers a memory page to or from the U-proc's flash device.
 *
 *  Parameters:
 *         pickedSwapPoolFrame – index of the memory frame
 *         devNo – flash device number
 *         blockNo – logical page number in the flash
 *         isRead – 1 for read, 0 for write
 *
 *  Returns:
 *
 **********************************************************/
void read_write_flash(int pickedSwapPoolFrame, support_t *currentSupport, int blockNo, int isRead) {
	int devNo = swapPoolTable[pickedSwapPoolFrame].ASID - 1;
	if(isRead == TRUE) {
		devNo = currentSupport->sup_asid - 1;
	}
	int flashSemIdx = devSemIdx(FLASHINT, devNo, FALSE);

	/* Get the device register address for the U-proc’s flash device */
	device_t *flashDevRegAdd = devAddrBase(FLASHINT, devNo);

	SYSCALL(PASSERN, &(mutex[flashSemIdx]), 0, 0);

	/* Write the physical memory address (start of frame) to DATA0 */
	flashDevRegAdd->d_data0 = (SWAP_POOL_START + (pickedSwapPoolFrame * PAGESIZE));

	/* Choose the correct flash command */
	int flashCommand;
	if(isRead == FALSE) {
		flashCommand = FLASHWRITE;
	} else {
		flashCommand = FLASHREAD;
	}

	/* Disable interrupts to ensure to do atomically */
	setSTATUS(getSTATUS() & (~IECBITON));
	/* Write the command to COMMAND register */
	flashDevRegAdd->d_command = (blockNo << COMMAND_SHIFT) | flashCommand;
	/* Block the process until the flash operation is complete */
	int flashStatus = SYSCALL(IOWAIT, FLASHINT, devNo, 0);
	/* Re-enable interrupts */
	setSTATUS(getSTATUS() | IECBITON);

	SYSCALL(VERHO, &(mutex[flashSemIdx]), 0, 0);

	if(flashStatus != READY) {
		program_trap_handler(currentSupport, &swapPoolSema4);
	}
}


HIDDEN void helper_copy_block(int *src, int *dst){
    int i;
    for (i = 0; i < (BLOCKSIZE/4); i++){
        *dst = *src;
        dst++; /*should increase by 4*/
        src++;
    }
}

void write_to_disk_for_pager(int devNo, int sectNo2D, int src, support_t *currentSupport){
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    int maxcyl = ((disk_dev_reg_addr->d_data1) >> 16) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> 8) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    if (sectNo2D > (maxcyl*maxhead*maxsect)){
        program_trap_handler(currentSupport, NULL);
    }

    SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
        int sectNo = (sectNo2D % (maxhead * maxsect)) % maxsect;
		int headNo = (sectNo2D % (maxhead * maxsect)) / maxsect; /*divide and round down*/
    	int cylNo = sectNo2D / (maxhead * maxsect);
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL; /*seek*/
            int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
            return 0 - disk_status;
        }
        disk_dev_reg_addr->d_data0 = src;
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + WRITEBLK_DSK; /*write*/
            disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);

    if (disk_status == READY){
        return disk_status;
    } else {
        return 0 - disk_status;
    }
}

HIDDEN void read_from_disk_for_pager(int devNo, int sectNo2D, int dst, support_t *currentSupport){
	int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    int maxcyl = ((disk_dev_reg_addr->d_data1) >> 16) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> 8) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    if (sectNo2D > (maxcyl*maxhead*maxsect)){
        program_trap_handler(currentSupport, NULL);
    }

    SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
        int sectNo = (sectNo2D % (maxhead * maxsect)) % maxsect;
		int headNo = (sectNo2D % (maxhead * maxsect)) / maxsect; /*divide and round down*/
    	int cylNo = sectNo2D / (maxhead*maxsect);
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL;
            int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
        if (disk_status != READY){
            SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
            return 0 - disk_status;
        }
        disk_dev_reg_addr->d_data0 = dst;
        setSTATUS(getSTATUS() & (~IECBITON));
            disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + READBLK_DSK;
            disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
        setSTATUS(getSTATUS() | IECBITON);
    SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);

    if (disk_status == READY){
        return disk_status;
    } else{
        return 0 - disk_status;
    }
}


/**********************************************************
 *  TLB_exception_handler
 *
 *  Handles page faults by loading the missing page into memory.
 *  Kicks out a page if memory is full and update page tables and TLB.
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 **********************************************************/
void TLB_exception_handler() {
	/* Obtain the pointer to the Current Process’s Support Structure. */
	support_t *currentSupport = SYSCALL(SUPPORTGET, 0, 0, 0);

	/* Determine the cause of the TLB exception. )*/
	int TLBcause = CauseExcCode(currentSupport->sup_exceptState[PGFAULTEXCEPT].s_cause);

	/* If the Cause is a TLB-Modification exception, treat this exception as a program trap */
	if(TLBcause == TLB_MOD) {
		program_trap_handler(currentSupport, NULL);
	}

	/* Gain mutual exclusion over the Swap Pool table. */
	SYSCALL(PASSERN, &swapPoolSema4, 0, 0);

	/* Determine the missing page number which is found in the saved exception state’s EntryHi */
	int missingVPN = (currentSupport->sup_exceptState[PGFAULTEXCEPT].s_entryHI >> VPN_SHIFT) & VPN_MASK;

	/* find page table index for later use */
	int pgTableIndex;
	if(missingVPN == UPROC_STACK_VPN) {
		pgTableIndex = PAGE_TABLE_SIZE - 1;
	} else {
		pgTableIndex = (missingVPN - STARTVPN);
	}

	/* Pick a frame, i, from the Swap Pool.*/
	int pickedFrame = page_replace();

	/* Determine if frame i is occupied; examine entry i in the Swap Pool table. */
	if((swapPoolTable[pickedFrame].ASID != -1)) {
		/* disable interrupts */
		setSTATUS(getSTATUS() & (~IECBITON));
		/* Update process x’s Page Table: mark Page Table entry k as not valid.
		This entry is easily accessible, since the Swap Pool table’s entry i contains a pointer to this Page Table entry. */
		pte_t *occupiedPgTable = swapPoolTable[pickedFrame].matchingPgTableEntry;

		occupiedPgTable->EntryLo = (DBITON & GBITOFF) & VBITOFF;
		/* Update the TLB, if needed. */
		TLBCLR();
		int write_out_pg_tbl;
		if(swapPoolTable[pickedFrame].pgNo == UPROC_STACK_VPN) {
			write_out_pg_tbl = PAGE_TABLE_SIZE - 1;
		} else {
			write_out_pg_tbl = (swapPoolTable[pickedFrame].pgNo - STARTVPN);
		}
		/* enable interrupts */
		setSTATUS(getSTATUS() | IECBITON);
		/* Update process x’s backing store.
		Treat any error status from the write operation as a program trap.*/
		if((occupiedPgTable->EntryLo & DBITON) == DBITON) { /* D bit set */

			/* isRead = 0 since we are writing */
			read_write_flash(pickedFrame, currentSupport, write_out_pg_tbl, FALSE);
			/* write_to_disk_for_pager(RESERVED_DISK_NO, 32*(swapPoolTable[pickedFrame].ASID - 1) + write_out_pg_tbl, SWAP_POOL_START + (pickedFrame * PAGESIZE), currentSupport); */
		}
	}

	/* Read the contents of the Current Process’s backingstore/flash device logical page p into frame i. */
	/* isRead = 1 since we are reading */
	read_write_flash(pickedFrame, currentSupport, pgTableIndex, TRUE);
	/* read_from_disk_for_pager(RESERVED_DISK_NO, 32*(currentSupport->sup_asid - 1) + pgTableIndex, SWAP_POOL_START + (pickedFrame * PAGESIZE), currentSupport); */

	/* Update the Swap Pool table’s entry i to reflect frame i’s new contents: page p belonging to the Current Process’s ASID,
	and a pointer to the Current Process’s Page Table entry for page p. */
	swapPoolTable[pickedFrame].ASID = currentSupport->sup_asid;
	swapPoolTable[pickedFrame].pgNo = missingVPN;
	swapPoolTable[pickedFrame].matchingPgTableEntry = &(currentSupport->sup_privatePgTbl[pgTableIndex]);

	setSTATUS(getSTATUS() & (~IECBITON));
	/* Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) and occupying frame i (PFN field).*/
	/* Set new PFN */
	swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo = (SWAP_POOL_START + (pickedFrame * PAGESIZE));
	/* Set V bit */
	swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo |= VBITON;
	/* Set D bit */
	swapPoolTable[pickedFrame].matchingPgTableEntry->EntryLo |= DBITON;

	/* Update the TLB. */
	TLBCLR();
	setSTATUS(getSTATUS() | IECBITON);

	/* Release mutual exclusion over the Swap Pool table. SYS4 */
	SYSCALL(VERHO, &swapPoolSema4, 0, 0);

	/* Return control to the Current Process */
	LDST((state_PTR) & (currentSupport->sup_exceptState[PGFAULTEXCEPT]));
}
