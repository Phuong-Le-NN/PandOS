/************************** INITPROC.H ******************************
 *
 *  The externals declaration file for INITPROC Module
 *
 *  Written by Phuong and Oghap on Mar 2025
 */

#ifndef INITPROC_H
#define INITPROC_H

#include "/usr/include/umps3/umps/libumps.h"

#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/types.h"
#include "../h/const.h"
#include "../phase5/delayDaemon.h"

extern void TLB_exception_handler();
extern void general_exception_handler();

void test();
int mutex[DEVINTNUM * DEVPERINT + DEVPERINT];
int masterSemaphore;

#endif