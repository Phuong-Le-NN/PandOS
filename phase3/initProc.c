/*********************************INITPROC.C*******************************
 *
 *  Implementation of the User Process Initialization Module
 *
 *  This module is responsible for initializing the user level support
 *  structures and page tables required for U-proc execution. It sets
 *  up the exception contexts, ASIDs, and program states for up to
 *  8 user processes, enabling virtual memory support and TLB handling.
 *
 *  The user process setup includes:
 *  - Initializing each U-proc’s private page table with proper VPN,
 *    ASID, and valid/dirty bits.
 *  - Assigning stack pointers and exception handlers for both TLB and
 *    general exceptions.
 *  - Creating the initial state for each process and invoking SYSCALL
 *    to create the PCB and insert it into the Ready Queue.
 *
 *      Modified by Phuong and Oghap on March 2025
 */

#include "initProc.h"

int masterSemaphore = 0;

/**********************************************************
 *  init_Uproc_pgTable
 *
 *  Initializes the page table for a user-level process.
 *  Sets EntryHi with VPN and ASID, and EntryLo with initial
 *  D, V, and G bit settings. Stack page is set separately.
 *
 *  Parameters:
 *         support_t *currentSupport – pointer to U-proc's support structure
 *
 *  Returns:
 *
 **********************************************************/
void init_Uproc_pgTable(support_t *currentSupport) {
	/* To initialize a Page Table one needs to set the VPN, ASID, V, and D bit fields for each Page Table entry*/
	int i;
	for(i = 0; i < PAGE_TABLE_SIZE; i++) {
		/* ASID field, for any given Page Table, will all be set to the U-proc’s unique ID*/
		currentSupport->sup_privatePgTbl[i].EntryHi = KUSEG + (i * PAGESIZE) + (currentSupport->sup_asid << ASID_SHIFT);
		currentSupport->sup_privatePgTbl[i].EntryLo = (DBITON & GBITOFF) & VBITOFF;
	}
	/* reset the idx 31 element */
	currentSupport->sup_privatePgTbl[PAGE_TABLE_SIZE - 1].EntryHi = UPROC_STACK_AREA + (currentSupport->sup_asid << ASID_SHIFT);
}

/**********************************************************
 *  init_Uproc
 *
 *  Initializes a U-proc's state, support structure, and
 *  exception contexts. Also sets up its page table and
 *  creates a new PCB by calling SYS1.
 *
 *  Parameters:
 *         support_t *initSupportPTR – pointer to support struct
 *         int ASID – unique ID of the user process
 *
 *  Returns:
 *
 **********************************************************/
int init_Uproc(support_t *initSupportPTR, int ASID) {
	state_t initState;

	initState.s_pc = UPROCSTARTADDR;
	initState.s_t9 = UPROCSTARTADDR;
	initState.s_sp = UPROCSTACK;
	initState.s_status = ((IEPBITON | TEBITON) | IPBITS) | KUPBITON;

	initState.s_entryHI = (ASID << ASID_SHIFT);

	initSupportPTR->sup_asid = ASID;

	initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)TLB_exception_handler;
	initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)general_exception_handler;

	initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_status = ((IEPBITON | TEBITON) | IPBITS) & KUPBITOFF;
	initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_status = ((IEPBITON | TEBITON) | IPBITS) & KUPBITOFF;

	initSupportPTR->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = &initSupportPTR->sup_stackTlb[TLB_STACK_AREA];
	initSupportPTR->sup_exceptContext[GENERALEXCEPT].c_stackPtr = &initSupportPTR->sup_stackGen[GEN_EXC_STACK_AREA];

	initSupportPTR->delaySem = 0;

	init_Uproc_pgTable(initSupportPTR);

	int newPcbStat = SYSCALL(1, &initState, initSupportPTR, 0);
	return newPcbStat;
}
/**********************************************************
 *  helper_copy_block
 *
 *  Copies a block of data from the source address to the 
 *  destination address. The block is copied in 4-byte 
 *  increments based on the block size.
 *
 *  Parameters:
 *         int *src – pointer to the source address
 *         int *dst – pointer to the destination address
 *
 *  Returns:
 *         None
 *
 **********************************************************/
void helper_copy_block(int *src, int *dst){
    int i;
    for (i = 0; i < (BLOCKSIZE/4); i++){
        *dst = *src;
        dst++; /*should increase by 4*/
        src++;
    }
}

/**********************************************************
 *  helper_read_flash
 *
 *  Reads a block of data from the flash storage. This function 
 *  calculates the appropriate memory address for the flash 
 *  storage and initiates a read operation.
 *
 *  Parameters:
 *         int devNo – the device number of the flash storage
 *         int blockNo – the block number to read from the flash storage
 *
 *  Returns:
 *         int – flash status (READY or error code)
 *
 **********************************************************/
int helper_read_flash(int devNo, int blockNo){
    int flash_sem_idx = devSemIdx(FLASHINT, devNo, FALSE);

    device_t *flash_dev_reg_addr = devAddrBase(FLASHINT, devNo);
    
    flash_dev_reg_addr->d_data0 = FLASK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo);
    setSTATUS(getSTATUS() & (~IECBITON));
    	flash_dev_reg_addr->d_command = (blockNo << BLOCKNUM_SHIFT) + READBLK_FLASH;
        int flash_status = SYSCALL(IOWAIT, FLASHINT, devNo, 0);
    setSTATUS(getSTATUS() | IECBITON);

    if (flash_status == READY){
        return flash_status;
    } else{
        return 0 - flash_status;
    }
}

/**********************************************************
 *  helper_write_disk
 *
 *  Writes a block of data to the disk. This function calculates 
 *  the appropriate cylinder, head, and sector numbers and 
 *  initiates a write operation.
 *
 *  Parameters:
 *         int secNo1D – the 1D sector number to write to
 *
 *  Returns:
 *         int – disk status (READY or error code)
 *
 **********************************************************/
int helper_write_disk(int secNo1D){
    int devNo = RESERVED_DISK_NO;
    int disk_sem_idx = devSemIdx(DISKINT, devNo, FALSE);

    device_t *disk_dev_reg_addr = devAddrBase(DISKINT, devNo);

    int maxcyl = ((disk_dev_reg_addr->d_data1) >> 16) & 0xFFFF;
    int maxhead = ((disk_dev_reg_addr->d_data1) >> 8) & 0xFF;
    int maxsect = (disk_dev_reg_addr->d_data1) & 0xFF;

    int sectNo = (secNo1D % (maxhead * maxsect)) % maxsect;
	int headNo = (secNo1D % (maxhead * maxsect)) / maxsect; /*divide and round down*/
    int cylNo = secNo1D / (maxhead * maxsect);
    setSTATUS(getSTATUS() & (~IECBITON));
        disk_dev_reg_addr->d_command = (cylNo << CYLNUM_SHIFT) + SEEKCYL; /*seek*/
        int disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
    setSTATUS(getSTATUS() | IECBITON);
    if (disk_status != READY){
        SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
        return 0 - disk_status;
    }
    disk_dev_reg_addr->d_data0 = DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo); /*carefull not to do anything with device between writing to data0 and command*/
    setSTATUS(getSTATUS() & (~IECBITON));
        disk_dev_reg_addr->d_command = (headNo << HEADNUM_SHIFT) + (sectNo << SECTNUM_SHIFT) + WRITEBLK_DSK; /*write*/
        disk_status = SYSCALL(IOWAIT, DISKINT, devNo, 0);
    setSTATUS(getSTATUS() | IECBITON);

    if (disk_status == READY){
        return disk_status;
    } else {
        return 0 - disk_status;
    }
}

/**********************************************************
 *  set_up_backing_store
 *
 *  Sets up the backing store by reading pages from flash 
 *  storage and writing them to disk. This function iterates 
 *  through all the pages of each device and performs the 
 *  reading and writing operations using the helper functions.
 *  Mutexes are used to ensure thread synchronization.
 *
 *  Parameters:
 *         None
 *
 *  Returns:
 *         None
 *
 **********************************************************/
void set_up_backing_store(){
	int devNo;
	int pageNo;

	int flash_sem_idx;
	int disk_sem_idx = devSemIdx(DISKINT, RESERVED_DISK_NO, FALSE);

	int flash_status;
	int disk_status;
	

	SYSCALL(PASSERN, &(mutex[disk_sem_idx]), 0, 0);
	for (devNo = 0; devNo < UPROC_NUM; devNo++){
		for (pageNo = 0; pageNo < PAGE_TABLE_SIZE; pageNo++){
			flash_sem_idx = devSemIdx(FLASHINT, devNo, FALSE);

			SYSCALL(PASSERN, &(mutex[flash_sem_idx]), 0, 0);

				/*read from flash*/
				flash_status = helper_read_flash(devNo, pageNo);
				/*copy from flahs buffer to dsk buffer*/
				helper_copy_block(FLASK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo), DISK_DMA_BUFFER_BASE_ADDR + (BLOCKSIZE*devNo));
				/*write to disk*/
				disk_status = helper_write_disk(32*devNo + pageNo);

			SYSCALL(VERHO, &(mutex[flash_sem_idx]), 0, 0);
		}
	}
	SYSCALL(VERHO, &(mutex[disk_sem_idx]), 0, 0);
}

/**********************************************************
 *  test
 *
 *  Function for initializing the system test.
 *  - Initializes swap structures and mutexes
 *  - Sets 8 user processes with init_Uproc()
 *  - Waits for all user processes to finish
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 **********************************************************/
void test() {

	int i;
	for(i = 0; i < (DEVINTNUM * DEVPERINT + DEVPERINT); i++) {
		mutex[i] = 1;
	}

	initSwapStruct();
	set_up_backing_store();
	initADL();

	support_t initSupportPTRArr[UPROC_NUM + 1]; /*1 extra sentinel node*/

	int newUprocStat;
	for(i = 1; i <= UPROC_NUM; i++) {
		newUprocStat = init_Uproc(&initSupportPTRArr[i - 1], i);
		if(newUprocStat == -1) {
			SYSCALL(TERMINATETHREAD, 0, 0, 0);
		}
	}

	for(i = 1; i <= UPROC_NUM; i++) {
		SYSCALL(PASSERN, &masterSemaphore, 0, 0); /* P operation */
	}

	SYSCALL(TERMINATETHREAD, 0, 0, 0);
}