#ifndef CONSTS
#define CONSTS

/****************************************************************************
 *
 * This header file contains utility constants & macro definitions.
 *
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE 4096 /* page size in bytes	*/
#define WORDLEN 4     /* word size in bytes	*/

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR 0x10000000
#define RAMBASESIZE 0x10000004
#define TODLOADDR 0x1000001C
#define INTERVALTMR 0x10000020
#define TIMESCALEADDR 0x10000024

/* utility constants */
#define TRUE 1
#define FALSE 0
#define HIDDEN static
#define EOS '\0'

#define NULL ((void *)0xFFFFFFFF)

/**********************************************************************************************
 * Interrupts related constants
 */

/* high precedence interrupts */
#define INTERPROCESSORINT 0
#define PLTINT 1
#define INTERVALTIMERINT 2

/* device interrupts */
#define DISKINT 3
#define FLASHINT 4
#define NETWINT 5
#define PRNTINT 6
#define TERMINT 7

#define DEVINTNUM 5   /* interrupt lines used by devices */
#define DEVPERINT 8   /* devices per interrupt line */
#define DEVREGLEN 4   /* device register field length in bytes, and regs per dev */
#define DEVREGSIZE 16 /* device register size in bytes */

/***********************************************************************************************
 * Device related constants
 */
/* device register field number for non-terminal devices */
#define STATUS 0
#define COMMAND 1
#define DATA0 2
#define DATA1 3

/* device register field number for terminal devices */
#define RECVSTATUS 0
#define RECVCOMMAND 1
#define TRANSTATUS 2
#define TRANCOMMAND 3

/* device common STATUS codes */
#define UNINSTALLED 0
#define READY 1
#define BUSY 3

/* device common COMMAND codes */
#define RESET 0
#define ACK 1

/**********************************************************************************************
 * Memory related constants
 */

#define KSEG0 0x00000000
#define KSEG1 0x20000000
#define KSEG2 0x40000000
#define KUSEG 0x80000000
#define RAMSTART 0x20000000
#define BIOSDATAPAGE 0x0FFFF000
#define PASSUPVECTOR 0x0FFFF900

#define BADADDR 0xFFFFFFFF

/**********************************************************************************************
 *  Exceptions related constants
 */
#define PGFAULTEXCEPT 0
#define GENERALEXCEPT 1

#define TLB_MOD 1
#define INT 0  /* External Device Interrupt*/
#define MOD 1  /* TLB-Modification Exception*/
#define TLBL 2 /* TLB Invalid Exception: on a Load instr. or instruction fetch*/
#define TLBS 3 /* TLB Invalid Exception: on a Store instr.*/
#define ADEL 4 /* Address Error Exception: on a Load or instruction fetch*/
#define ADES 5 /* Address Error Exception: on a Store instr.*/
#define IBE 6  /* Bus Error Exception: on an instruction fetch*/
#define DEB 7  /* Bus Error Exception: on a Load/Store data access*/
#define SYS 8  /* Syscall Exception*/
#define BP 9   /* Breakpoint Exception*/
#define RI 10  /* Reserved Instruction Exception*/
#define CPU 11 /* Coprocessor Unusable Exception*/
#define OV 12  /* Arithmetic Overflow Exception*/

#define BUSERROR 6
#define RESVINSTR 10
#define ADDRERROR 4
#define SYSCALLEXCPT 8

/**********************************************************************************************
 * SYSCALL relted constants
 */

#define CREATETHREAD 1
#define TERMINATETHREAD 2
#define PASSERN 3
#define VERHO 4
#define IOWAIT 5
#define CPUTIMEGET 6
#define CLOCKWAIT 7
#define SUPPORTGET 8

#define CLOCKINTERVAL 100000UL /* interval to V clock semaphore */
#define SYSCAUSE (0x8 << 2)

/**********************************************************************************************
 *  hardware constants
 */
#define PRINTCHR 2
#define BYTELEN 8
#define RECVD 5

#define TERMSTATMASK 0xFF
#define CAUSEMASK 0xFF
#define VMOFF 0xF8FFFFFF

#define QPAGE 1024

/**********************************************************************************************
 *  Cause register bit fields
 */
#define EXECCODEBITS 0x0000007C
#define IPBITS 0x0000FF00
#define IECBITON 0x00000001
#define KUPBITON 0x00000008
#define KUPBITOFF 0xFFFFFFF7
#define IEPBITON 0x00000004
#define TEBITON 0x08000000
#define ALLOFF 0x0

/* Device register related addresses*/
#define INSTALLED_DEV_REG 0x1000002C
#define INT_DEV_REG 0x10000040

/* Support level data structures related constants */
#define VPN_SHIFT 12
#define VPN_MASK 0x000FFFFF
#define SWAP_POOL_SIZE 32
#define SWAP_POOL_START 0x20020000 + PAGESIZE*16
#define PAGE_TABLE_SIZE 32
#define ASID_SHIFT 6
#define UPROC_NUM 8
#define UPROC_STACK_AREA 0xBFFFF000
#define LAST_USER_PAGE 0x8001E000
#define TLB_STACK_AREA 499
#define GEN_EXC_STACK_AREA 499

/* Constant bits for ENTRYHI and ENTRYLOW */
#define DBITON 0x00000400
#define VBITON 0x00000200
#define GBITON 0x00000100
#define DBITOFF 0xFFFFFBFF
#define GBITOFF 0xFFFFFEFF
#define VBITOFF 0xFFFFFDFF
#define PFN_MASK 0xFFFFF000

/* U-PROC constants */
#define UPROCSTARTADDR 0x800000B0
#define UPROCSTACK 0xC0000000
#define STARTVPN 0x80000
#define UPROC_STACK_VPN 0xBFFFF

/* READ/WRITE constants */
#define NEW_LINE 10
#define STR_MIN 0
#define STR_MAX 128
#define STATUS_CHAR_MASK 0x000000FF
#define CHAR_TRANSMITTED 5
#define CHAR_RECIEVED 5
#define TRANSMIT_COMMAND 2
#define RECEIVE_COMMAND 2
#define RECEIVE_CHAR_MASK 0x0000FF00
#define CHAR_SHIFT 8
#define FLASHWRITE 3
#define FLASHREAD 2
#define TRANS_COMMAND_SHIFT 8
#define RECEIVE_COMMAND_SHIFT 8
#define COMMAND_SHIFT 8

/**********************************************************************************************
 * Macros
 */
#define MIN(A, B) ((A) < (B) ? A : B)

#define MAX(A, B) ((A) < (B) ? B : A)

#define ALIGNED(A) (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T) ((*((cpu_t *)INTERVALTMR)) = (T) * (*((cpu_t *)TIMESCALEADDR)))

/* Macro to read the TOD clock */
#define STCK(T) ((T) = ((*((cpu_t *)TODLOADDR)) / (*((cpu_t *)TIMESCALEADDR))))

/* Macro to calculate starting address of the device’s device register*/
#define devAddrBase(intLineNo, devNo) 0x10000054 + ((intLineNo - 3) * 0x80) + (devNo * 0x10);

/* Macro to get the ExcCode given the Cause register*/
#define CauseExcCode(Cause) (EXECCODEBITS & Cause) >> 2;

/* Macro to calculate address of Interrupt Device Bit Map of Interrupt line */
#define intDevBitMap(intLine) INT_DEV_REG + (0x04 * (intLine - 3));

/* Macro to calculate the device sema4 idx in the device sema4 array
   terminal write higher priority than terminal read*/
#define devSemIdx(intLineNo, devNo, termRead) (intLineNo - 3) * 8 + termRead * 8 + devNo;

/* Maximum number of semaphore and pcb that can be allocated*/
#define MAXPROC 20
#define MAXSEM MAXPROC

#define BLOCKSIZE   PAGESIZE

#define DISK_DMA_BUFFER_BASE_ADDR   0x20020000
#define FLASK_DMA_BUFFER_BASE_ADDR  0x20020000 + BLOCKSIZE*8

#define READBLK_DSK     3
#define WRITEBLK_DSK    4
#define SEEKCYL         2

#define SECTNUM_SHIFT   8
#define CYLNUM_SHIFT    8
#define HEADNUM_SHIFT   16

#define READBLK_FLASH   2
#define WRITEBLK_FLASH  3

#define BLOCKNUM_SHIFT  8

#define RESERVED_DISK_NO 0

#define MAXCYL_SHIFT 16
#define MAXHEAD_SHIFT 8

#endif
