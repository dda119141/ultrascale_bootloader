/******************************************************************************
* Copyright (c) 2014 - 2022 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*****************************************************************************/
/**
* @file boot.S
*
* @addtogroup a53_64_boot_code Cortex A53 64bit Processor Boot Code
* @{
* <h2> boot.S </h2>
*
* The boot code performs minimum configuration which is required for an
* application. Cortex-A53 starts by checking current exception level. If the
* current exception level is EL3 and BSP is built for EL3, it will do
* initialization required for application execution at EL3. Below is a
* sequence illustrating what all configuration is performed before control
* reaches to main function for EL3 execution.
*
* 1. Program vector table base for exception handling
* 2. Set reset vector table base address
* 3. Program stack pointer for EL3
* 4. Routing of interrupts to EL3
* 5. Enable ECC protection
* 6. Program generic counter frequency
* 7. Invalidate instruction cache, data cache and TLBs
* 8. Configure MMU registers and program base address of translation table
* 9. Transfer control to _start which clears BSS sections and runs global
*    constructor before jumping to main application
*
* If the current exception level is EL1 and BSP is also built for EL1_NONSECURE
* it will perform initialization required for application execution at EL1
* non-secure. For all other combination, the execution will go into infinite
* loop. Below is a sequence illustrating what all configuration is performed
* before control reaches to main function for EL1 execution.
*
* 1. Program vector table base for exception handling
* 2. Program stack pointer for EL1
* 3. Invalidate instruction cache, data cache and TLBs
* 4. Configure MMU registers and program base address of translation table
* 5. Transfer control to _start which clears BSS sections and runs global
*    constructor before jumping to main application
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who     Date     Changes
* ----- ------- -------- ---------------------------------------------------
* 5.00  pkp	05/21/14 Initial version
* 6.00	pkp     07/25/16 Program the counter frequency
* 6.02  pkp	01/22/17 Added support for EL1 non-secure
* 6.02	pkp	01/24/17 Clearing status of FPUStatus variable to ensure it
*			 holds correct value.
* 6.3   mus 04/20/17 CPU Cache protection bit in the L2CTLR_EL1 will be in
*                    set state on reset. So, setting that bit through boot
*                    code is redundant, hence removed the code which sets
*                    CPU cache protection bit.
* 6.4   mus      08/11/17 Implemented ARM erratum 855873.It fixes
*                         CR#982209.
* 6.6   mus      01/19/18 Added isb after writing to the cpacr_el1/cptr_el3,
*                         to ensure floating-point unit is disabled, before
*                         any subsequent instruction.
* 7.0   mus      03/26/18 Updated TCR_EL3/TCR_EL1 as per versal address map
* 7.3   mus      04/24/20 Corrected CPACR_EL1 handling at EL1 NS
* 8.0   mus      07/06/21 Added support for CortexA78 processor in VERSAL NET SoC
* 8.0   mus      10/05/21 Default translation table for VERSAL NET has been configured
*                         for 256 TB address space, due to this page tables size
*                         exceeds OCM size, hence executable size is too large
*                         to fit into OCM. This patch adds option  to reduce
*                         page table size, when OCM_ELF flag is defined in
*                         compiler flags, translation table would be configured
*                         for 1 TB address space. It would help to reduce
*                         executable size.
*
* </pre>
*
******************************************************************************/

#include "xparameters.h"
#include "bspconfig.h"
#include "xil_errata.h"

.globl MMUTableL0
.globl MMUTableL1
.globl MMUTableL2
.global _prestart
.global _boot

.global __el3_stack
.global _vector_table
.global invalidate_dcaches

.set EL3_stack,		__el3_stack

.set TT_S1_FAULT,	0x0
.set TT_S1_TABLE,	0x3

.set L0Table,	MMUTableL0
.set L1Table,	MMUTableL1
.set L2Table,	MMUTableL2
.set vector_base,	_vector_table
.set rvbar_base,	0xFD5C0040

.set counterfreq,	XPAR_CPU_CORTEXA53_0_TIMESTAMP_CLK_FREQ
.set MODE_EL1, 0x5
.set DAIF_BIT,	0x1C0

.section .boot,"ax"

/* this initializes the various processor modes */

_prestart:
_boot:
	mov      x0, #0
	mov      x1, #0
	mov      x2, #0
	mov      x3, #0
	mov      x4, #0
	mov      x5, #0
	mov      x6, #0
	mov      x7, #0
	mov      x8, #0
	mov      x9, #0
	mov      x10, #0
	mov      x11, #0
	mov      x12, #0
	mov      x13, #0
	mov      x14, #0
	mov      x15, #0
	mov      x16, #0
	mov      x17, #0
	mov      x18, #0
	mov      x19, #0
	mov      x20, #0
	mov      x21, #0
	mov      x22, #0
	mov      x23, #0
	mov      x24, #0
	mov      x25, #0
	mov      x26, #0
	mov      x27, #0
	mov      x28, #0
	mov      x29, #0
	mov      x30, #0

OKToRun:
	mrs	x0, currentEL
	cmp	x0, #0xC
	beq	InitEL3

	b 	error			// go to error if current exception level is neither EL3 nor EL1

error: 	b	error

InitEL3:
	/*Set vector table base address*/
	ldr	x1, = vector_base
	msr	VBAR_EL3,x1

	/* Set reset vector address */
	/* Get the cpu ID */
	mrs  x0, MPIDR_EL1
	and  x0, x0, #0xFF
	mov  w0, w0

	ldr	 w2, =rvbar_base
	/* calculate the rvbar base address for particular CPU core */
	mov	 w3, #0x8
	mul	 w0, w0, w3
	add	 w2, w2, w0
	/* store vector base address to RVBAR */
	str  x1, [x2]

	/*Define stack pointer for current exception level*/
	ldr	 x2,=EL3_stack
	mov	 sp,x2

	/* Enable Trapping of SIMD/FPU register for standalone BSP */
	mov      x0, #0
	orr      x0, x0, #(0x1 << 10)
	msr      CPTR_EL3, x0
	isb

	/*
	 * Clear FPUStatus variable to make sure that it contains current
	 * status of FPU i.e. disabled. In case of a warm restart execution
	 * when bss sections are not cleared, it may contain previously updated
	 * value which does not hold true now.
	 */
	 ldr x0,=FPUStatus
	 str xzr, [x0]

	/* Asynchronous exception routing determines which exception level is used to
	 * handle an exception.
	 * To route an asynchronous exception to EL3, SCR_EL3 needs to be set
	 */
	/* Configure SCR_EL3 */
	mov      w1, #0              	//; Initial value of register is unknown
	orr      w1, w1, #(1 << 11)  	//; Set ST bit (Secure EL1 can access CNTPS_TVAL_EL1, CNTPS_CTL_EL1 & CNTPS_CVAL_EL1)
	orr      w1, w1, #(1 << 10)  	//; Set RW bit (EL1 is AArch64, as this is the Secure world)
	orr      w1, w1, #(1 << 3)   	//; Set EA bit (SError routed to EL3)
	orr      w1, w1, #(1 << 2)   	//; Set FIQ bit (FIQs routed to EL3)
	orr      w1, w1, #(1 << 1)   	//; Set IRQ bit (IRQs routed to EL3)
	msr      SCR_EL3, x1
	
	/*configure cpu auxiliary control register EL1 */
	ldr	x0,=0x80CA000 		// L1 Data prefetch control - 5, Enable device split throttle, 2 independent data prefetch streams
#if CONFIG_ARM_ERRATA_855873
        /*
	 *  Set ENDCCASCI bit in CPUACTLR_EL1 register, to execute data
	 *  cache clean operations as data cache clean and invalidate
	 *
	 */
        orr     x0, x0, #(1 << 44)      //; Set ENDCCASCI bit
#endif
	msr	S3_1_C15_C2_0, x0 	//CPUACTLR_EL1

	/* program the counter frequency */
	ldr	x0,=counterfreq
	msr	CNTFRQ_EL0, x0

	/*Enable hardware coherency between cores*/
	mrs      x0, S3_1_c15_c2_1  	//Read EL1 CPU Extended Control Register
	orr      x0, x0, #(1 << 6)  	//Set the SMPEN bit
	msr      S3_1_c15_c2_1, x0  	//Write EL1 CPU Extended Control Register
	isb

	tlbi 	ALLE3
	ic      IALLU                  	//; Invalidate I cache to PoU
	bl 	invalidate_dcaches
	dsb	 sy
	isb

	b 	 _startup		//jump to start


