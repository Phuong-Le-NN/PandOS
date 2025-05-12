/************************** VMSUPPORT.H ******************************
 *
 *  The externals declaration file for VMSUPPORT Module
 *
 *  Written by Phuong and Oghap on Mar 2025
 */

#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"

#include "../phase2/initial.h"

extern int mutex[DEVINTNUM * DEVPERINT + DEVPERINT];
extern void program_trap_handler(support_t *passedUpSupportStruct, semd_t *heldSemd);

/* global variables */
swapPoolFrame_t swapPoolTable[SWAP_POOL_SIZE];
int swapPoolSema4;

void initSwapStruct();
void uTLB_RefillHandler();
void TLB_exception_handler();

#endif