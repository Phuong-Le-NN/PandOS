#$Id: Makefile,v 1.2 2004/05/01 14:53:48 morsiani Exp morsiani $
# Makefile for mips-linux
#
# Edited for uMPS v3 by MikeyG on 2020/04/14

# Simplistic search for the umps3 installation prefix.
# If you have umps3 installed on some weird location, set UMPS3_DIR_PREFIX by hand.
ifneq ($(wildcard /usr/bin/umps3),)
	UMPS3_DIR_PREFIX = /usr
	LIBDIR = $(UMPS3_DIR_PREFIX)/lib/x86_64-linux-gnu/umps3
	
else
	UMPS3_DIR_PREFIX = /usr/local
	LIBDIR = $(UMPS3_DIR_PREFIX)/lib/umps3
endif

INCDIR = $(UMPS3_DIR_PREFIX)/include/umps3/umps
SUPDIR = $(UMPS3_DIR_PREFIX)/share/umps3
#LIBDIR = $(UMPS3_DIR_PREFIX)/lib/umps3

DEFS = ../h/const.h ../h/types.h ../h/pcb.h ../h/asl.h \
	../h/initial.h ../h/interrupts.h ../h/scheduler.h ../h/exceptions.h \
	../h/initProc.h ../h/vmSupport.h ../h/sysSupport.h \
	$(INCDIR)/libumps.h Makefile

OBJS = ../phase1/asl.o ../phase1/pcb.o \
       ../phase2/initial.o ../phase2/interrupts.o ../phase2/scheduler.o ../phase2/exceptions.o \
       initProc.o vmSupport.o sysSupport.o \
	   ../phase5/delayDaemon.o \

CFLAGS = -ffreestanding -ansi -Wall -c -mips1 -mabi=32 -mfp32 -mno-gpopt -G 0 -fno-pic -mno-abicalls

LDAOUTFLAGS = -G 0 -nostdlib -T $(SUPDIR)/umpsaout.ldscript
LDCOREFLAGS =  -G 0 -nostdlib -T $(SUPDIR)/umpscore.ldscript


CC = mipsel-linux-gnu-gcc
LD = mipsel-linux-gnu-ld
AS = mipsel-linux-gnu-as -KPIC

EF = umps3-elf2umps
UDEV = umps3-mkdev

#main target
all: kernel.core.umps 

kernel.core.umps: kernel
	$(EF) -k kernel

kernel: $(OBJS)
	$(LD) $(LDCOREFLAGS) $(LIBDIR)/crtso.o $(OBJS) $(LIBDIR)/libumps.o -o kernel

%.o: %.c $(DEFS)
	$(CC) $(CFLAGS) $<


clean:
	rm -f *.o *.umps kernel ../phase1/*.o ../phase1/*.umps ../phase1/kernel ../phase2/*.o ../phase2/*.umps ../phase2/kernel


distclean: clean
	-rm kernel.*.umps tape0.umps

TERMS = term0.umps term1.umps term2.umps term3.umps term4.umps term5.umps term6.umps term7.umps
PRINTS = print0.umps print1.umps print2.umps print3.umps print4.umps print5.umps print6.umps print7.umps

# Target to create all term files
# Combined target
devices: $(TERMS) $(PRINTS)
# Generic rule for both termX.umps and printX.umps
%.umps:
	touch $@
