/*******************************DELAYDAEMON.C*******************************
 *  Implementation of the Delay Facility and Delay Daemon Process
 *      Maintains a sorted NULL-terminated singly linked list of delay 
 *      event descriptor nodes (Active Delay List - ADL), pointed to by delayd_h.
 *      Also maintains a free list, delaydFree_h, which contains unused 
 *      delay descriptor nodes from the statically allocated delaydTable[].
 *      Two dummy nodes are used in the ADL for easier insertion and traversal:
 *      one with wakeTime -1 (head) and one with wakeTime MAXINT (tail).
 *      Provides the implementation of the SYS18 service, allowing U-procs 
 *      to delay execution for a specified number of seconds by blocking 
 *      on their private semaphores.
 *      Includes the Delay Daemon, a kernel process that periodically (every 100ms)
 *      checks the ADL and unblocks any U-procs whose wakeTime has expired.
 *      Public functions:
 *          - initADL(): Initializes the ADL, free list, and launches Delay Daemon.
 *          - DELAY(): Implements SYS18; delays the calling U-proc.
 *      Hidden functions include:
 *          - allocDelayd(), freeDelayd(), traverseADL(): for ADL maintenance.
 *          - delay_daemon(): Infinite loop executed by the Delay Daemon.
 *      Mutual exclusion on the ADL is ensured via ADL_mutex, a binary semaphore.
 *      Modified by Phuong Le 2025
 */


#include "../h/pcb.h"
#include "delayDaemon.h"

HIDDEN delayd_t *delaydFree_h;
HIDDEN delayd_t *delayd_h;
HIDDEN int ADL_mutex;

/**********************************************************
 *  Freeing a delay descriptor and adding it back to the delaydFree_h list
 *
 *  Parameters:
 *         delayd_t *d : pointer to the delay descriptor to be freed
 *
 *  Returns:
 *         void
 *
 */
HIDDEN void freeDelayd(delayd_t *d) {
	d->d_supStruct = NULL;
	d->d_next = delaydFree_h;
	d->d_wakeTime = 0;
	delaydFree_h = d;
}

/**********************************************************
 *  Allocating and initializing a delay descriptor from the delaydFree_h list
 *
 *  Parameters:
 *         int wakeTime : the wake-up time (in microseconds) for the delay descriptor
 *         support_t *currentSupport : pointer to the support structure of the requesting process
 *
 *  Returns:
 *         delayd_t *allocatedDelayd : pointer to the allocated and initialized delay descriptor
 *         NULL if the free list is empty (allocation fails)
 *
 */
HIDDEN delayd_t *allocDelayd(int wakeTime, support_t *currentSupport) {
	/*special case no free delayD to allocate*/
	if(delaydFree_h == NULL) {
		return NULL;
	}
	/*removing the first wakeTime in delaydFree_h*/
	delayd_t *allocatedDelayd = delaydFree_h;

	delaydFree_h = delaydFree_h->d_next;

	/*initialize its attributes*/
	allocatedDelayd->d_next = NULL;
	allocatedDelayd->d_wakeTime = wakeTime;
	allocatedDelayd->d_supStruct = currentSupport;

	return allocatedDelayd;
}

/**********************************************************
 *  Helper function: Traverse the Active Delay List (ADL) to find the proper predecessor
 *  for inserting a new delay descriptor based on wakeTime order
 *
 *  Parameters:
 *         int wakeTime : the wake-up time to use for insertion lookup
 *
 *  Returns:
 *         delayd_t *traverse : pointer to the delay descriptor before the correct insertion point
 *
 */
HIDDEN delayd_t *traverseADL(int wakeTime) {
	delayd_t *traverse = delayd_h;
	/*the loop that move traverse pointer until the the next wakeTime no longer has the descriptor smaller than the given wakeTime descriptor*/
	while((traverse->d_next->d_wakeTime != MAXSIGNEDINT) && (traverse->d_next->d_wakeTime < wakeTime)) {
		traverse = traverse->d_next;
	}
	return traverse;
}

/**********************************************************
 *  SYS18 implementation: Delay the calling process by a time duration
 *
 *  Parameters:
 *         support_t *currentSupport : pointer to the support structure of the calling process
 *
 *  Returns:
 *         void
 *
 */
void DELAY(support_t *currentSupport) {
	/*Converts s_a1 into microseconds delay*/
	int wakeTime = currentSupport->sup_exceptState[GENERALEXCEPT].s_a1 * 1000000;

	/*invalid wake time*/
	if((wakeTime < 0) || (wakeTime > MAXSIGNEDINT)) {
		SYSCALL(TERMINATEUSERPROC, 0, 0, 0);
	}

	/*Gain mutual exclusion over shared ADL structure*/
	SYSCALL(PASSERN, &ADL_mutex, 0, 0);
	/*Allocates and inserts a delay descriptor into the ADL*/
	delayd_t *newDelayd = allocDelayd(wakeTime, currentSupport);
	if(newDelayd == NULL) {
		SYSCALL(VERHO, &ADL_mutex, 0, 0);
		SYSCALL(TERMINATEUSERPROC, 0, 0, 0); /*fail to allocate*/
	}

	delayd_t *predecessor = traverseADL(wakeTime); /*look for the predecessor*/
	newDelayd->d_next = predecessor->d_next;
	predecessor->d_next = newDelayd;

	setSTATUS(getSTATUS() & (~IECBITON));
	/*RElease ADL mutex before blocking this process*/
	SYSCALL(VERHO, &ADL_mutex, 0, 0);

	/*Blocks the process on its private delay semaphore - this call scheduler and launch the next in ReadyQ*/
	SYSCALL(PASSERN, &(currentSupport->delaySem), 0, 0);
	setSTATUS(getSTATUS() | IECBITON);

	/*update PC to the instruction to run after this proc is awoken*/
	currentSupport->sup_exceptState[GENERALEXCEPT].s_pc += INSSIZE; 
	LDST(&(currentSupport->sup_exceptState[GENERALEXCEPT]));
}

/**********************************************************
 *  The Delay Daemon kernel process
 *  Wakes up processes whose delay time has expired
 *
 *  Parameters:
 *         void
 *
 *  Returns:
 *         void (never returns; runs in an infinite loop)
 */
HIDDEN void delay_daemon() {
	int currTOD;
	delayd_t *to_be_wake;
	while(TRUE == TRUE) {
		SYSCALL(CLOCKWAIT, 0, 0, 0); /*Periodically wakes every 100ms*/

		SYSCALL(PASSERN, &ADL_mutex, 0, 0); /*Acquires ADL_mutex and checks ADL*/
		STCK(currTOD);
		/*this loop break when the second node in the ADL no longer need to be wakened up*/
		while(delayd_h->d_next->d_wakeTime <= currTOD) { 
			to_be_wake = delayd_h->d_next;
			/*Unblocks any process whose wakeTime has passed*/
			SYSCALL(VERHO, &(to_be_wake->d_supStruct->delaySem), 0, 0); 
			/*Removes and frees the corresponding delay descriptors*/
			delayd_h->d_next = to_be_wake->d_next;
			freeDelayd(to_be_wake);

			STCK(currTOD);
		}
		SYSCALL(VERHO, &ADL_mutex, 0, 0);
	}
}

/**********************************************************
 *  Initializes the Active Delay List (ADL) and its Delay Daemon
 *
 *  Parameters:
 *         void
 *
 *  Returns:
 *         void
 */
void initADL() {
	static delayd_t delaydTable[MAXPROC + NUMBER_OF_SENTINEL];
	delayd_h = NULL;
	ADL_mutex = 1;
	/*adding the first from delaydTable to the delaydFree_h list*/
	delaydFree_h = &(delaydTable[0]);
	int i;
	/*adding and connecting the rest wakeTime from delaydTable to the delaydFree_h list*/
	for(i = 0; i < MAXPROC + 1; i = i + 1) {
		(delaydTable[i]).d_next = &(delaydTable[i + 1]);
	}
	/*making the d_next pointer of the tail node NULL*/
	(delaydTable[i]).d_next = NULL;

	/*allocating and adding the two dummy node into the ADL*/
	delayd_t *headDummy = allocDelayd(FIRSTINVALIDTIME, NULL);
	delayd_t *tailDummy = allocDelayd(MAXSIGNEDINT, NULL);

	headDummy->d_next = tailDummy;
	delayd_h = headDummy;

	/* state for the deamon */
	state_t daemonState;
	daemonState.s_pc = (memaddr)delay_daemon;
	daemonState.s_t9 = (memaddr)delay_daemon;
	/* assigning memory right below test stack page */
	daemonState.s_sp = ((devregarea_t *)RAMBASEADDR)->rambase + ((devregarea_t *)RAMBASEADDR)->ramsize - PAGESIZE;
	daemonState.s_status = (IEPBITON & KUPBITOFF) | IPBITS;
	daemonState.s_entryHI = DAEMON_PROC_ASID << ASID_SHIFT;
	/*Starts the Delay Daemon as a kernel process using SYSCALL(1)*/
	SYSCALL(CREATETHREAD, &daemonState, NULL, 0);
}