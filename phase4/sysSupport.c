/*********************************SYSSUPPORT.C*******************************
 *
 *  Implementation of the System Support Exception Handlers Module
 *
 *  This module defines exception handlers used by user processes
 *  to manage system calls, general exceptions, and program traps.
 *  It supports pass-up handling and interaction with the
 *  operating system kernel through the support structure.
 *
 *  The system support includes:
 *  - A SYSCALL exception handler that differentiates between valid
 *    system calls and illegal operations from user processes.
 *  - A Program Trap handler that deals with undefined or illegal
 *    instructions executed by a user process.
 *  - A General Exception handler that dispatches to appropriate
 *    handlers or terminates the process if the exception is unhandled.
 *
 *      Modified by Phuong and Oghap on March 2025
 */

#include "sysSupport.h"

/**********************************************************
 *  helper_return_control
 *
 *  Advances the program counter and returns control to the
 *  user process by restoring its exception state.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void helper_return_control(support_t *passedUpSupportStruct) {
	passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_pc += 4;
	LDST(&(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT]));
}

/**********************************************************
 *  program_trap_handler
 *
 *  Handles program trap exceptions by terminating the user
 *  process cleanly.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void program_trap_handler(support_t *passedUpSupportStruct, semd_t *heldSemd) {
	/*release any mutexes the U-proc might be holding.
	perform SYS9 (terminate) the process cleanly.*/
	if(heldSemd != NULL) {
		SYSCALL(VERHO, heldSemd, 0, 0);
	}
	TERMINATE(passedUpSupportStruct);
}

/**********************************************************
 *  TERMINATE
 *
 *  Terminates a user process. Releases its occupied frames,
 *  marks pages invalid, and performs SYS2 to kill the process.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void TERMINATE(support_t *passedUpSupportStruct) {
	/* Disable interrupts before touching shared structures */
	setSTATUS(getSTATUS() & (~IECBITON));
	int i;
	/* mark all of the frames it occupied as unoccupied */
	SYSCALL(PASSERN, &swapPoolSema4, 0, 0);
	for(i = 0; i < SWAP_POOL_SIZE; i++) {
		if(swapPoolTable[i].ASID == passedUpSupportStruct->sup_asid) {
			swapPoolTable[i].ASID = -1;
			swapPoolTable[i].pgNo = -1;
			swapPoolTable[i].matchingPgTableEntry = NULL;
		}
	}
	SYSCALL(VERHO, &swapPoolSema4, 0, 0);

	/* Mark pages as invalid (clear VALID bit) */
	for(i = 0; i < PAGE_TABLE_SIZE; i++) {
		passedUpSupportStruct->sup_privatePgTbl[i].EntryLo &= ~(PFN_MASK + VBITON);
	}

	/* Re-enable interrupts */
	setSTATUS(getSTATUS() | IECBITON);

	SYSCALL(VERHO, &masterSemaphore, 0, 0);

	/* Terminate the process */
	SYSCALL(TERMINATETHREAD, 0, 0, 0); /* SYS2 */
}

/**********************************************************
 *  GET_TOD
 *
 *  Retrieves the current time-of-day from the system clock
 *  and stores it in the user process's return register.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void GET_TOD(support_t *passedUpSupportStruct) {
	STCK(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_v0);
}

/**********************************************************
 *  syscall_handler
 *
 *  Dispatches system calls from user processes. Handles SYS9 to SYS13.
 *  If an unknown system call is encountered, invokes the trap handler.
 *
 *  Parameters:
 *         support_t *passedUpSupportStruct – pointer to the support struct
 *
 *  Returns:
 *
 **********************************************************/
void syscall_handler(support_t *passedUpSupportStruct) {
	switch(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_a0) {
		case 9:
			TERMINATE(passedUpSupportStruct);
		case 10:
			GET_TOD(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 11:
			WRITE_TO_PRINTER(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 12:
			WRITE_TO_TERMINAL(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 13:
			READ_FROM_TERMINAL(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 14:
			WRITE_TO_DISK(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 15:
			READ_FROM_DISK(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 16:
			WRITE_TO_FLASH(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 17:
			READ_FROM_FLASH(passedUpSupportStruct);
			helper_return_control(passedUpSupportStruct);
		case 18:
			DELAY(passedUpSupportStruct);
		default: /*the case where the process tried to do SYS 8- in user mode*/
			program_trap_handler(passedUpSupportStruct, NULL);
	}
}

/**********************************************************
 *  general_exception_handler
 *
 *  Determines the type of general exception and routes it to
 *  either the syscall handler or program trap handler.
 *
 *  Parameters:
 *
 *
 *  Returns:
 *
 **********************************************************/
void general_exception_handler() {
	support_t *passedUpSupportStruct = SYSCALL(SUPPORTGET, 0, 0, 0);
	/* like in phase2 how we get the exception code*/
	int excCode = CauseExcCode(passedUpSupportStruct->sup_exceptState[GENERALEXCEPT].s_cause);
	/* examine the sup_exceptState's Cause register ... pass control to either the Support Level's SYSCALL exception handler, or the support Level's Program Trap exception handler */
	if(excCode == SYS) {
		syscall_handler(passedUpSupportStruct);
	}
	program_trap_handler(passedUpSupportStruct, NULL);
}