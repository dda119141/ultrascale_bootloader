/******************************************************************************
* Copyright (c) 2014 - 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*****************************************************************************/
/**
* @file asm_vectors.S
*
* This file contains the initial vector table for the Cortex A53 processor
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who     Date     Changes
* ----- ------- -------- ---------------------------------------------------

* @note
*
* None.
*
******************************************************************************/

#include "bspconfig.h"

.org 0
.text

.globl _boot
.globl _vector_table

.globl FIQInterrupt
.globl IRQInterrupt
.globl SErrorInterrupt
.globl SynchronousInterrupt
.globl FPUStatus

/*
 * FPUContextSize is the size of the array where floating point registers are
 * stored when required. The default size corresponds to the case when there is no
 * nested interrupt. If there are nested interrupts in application which are using
 * floating point operation, the size of FPUContextSize need to be increased as per
 * requirement
 */

.set FPUContextSize, 528

.macro macro_saveregister
	stp	X0,X1, [sp,#-0x10]!
	stp	X2,X3, [sp,#-0x10]!
	stp	X4,X5, [sp,#-0x10]!
	stp	X6,X7, [sp,#-0x10]!
	stp	X8,X9, [sp,#-0x10]!
	stp	X10,X11, [sp,#-0x10]!
	stp	X12,X13, [sp,#-0x10]!
	stp	X14,X15, [sp,#-0x10]!
	stp	X16,X17, [sp,#-0x10]!
	stp 	X18,X19, [sp,#-0x10]!
	stp 	X29,X30, [sp,#-0x10]!
.endm

.macro macro_restoreregister
	ldp 	X29,X30, [sp], #0x10
	ldp 	X18,X19, [sp], #0x10
	ldp	X16,X17, [sp], #0x10
	ldp	X14,X15, [sp], #0x10
	ldp	X12,X13, [sp], #0x10
	ldp	X10,X11, [sp], #0x10
	ldp	X8,X9, [sp], #0x10
	ldp	X6,X7, [sp], #0x10
	ldp	X4,X5, [sp], #0x10
	ldp	X2,X3, [sp], #0x10
	ldp	X0,X1, [sp], #0x10
.endm

.macro macro_savefloatregister
/* Load the floating point context array address from FPUContextBase */
	ldr	x1,=FPUContextBase
	ldr	x0, [x1]

/* Save all the floating point register to the array */
	stp	q0,q1, [x0], #0x20
	stp	q2,q3, [x0], #0x20
	stp	q4,q5, [x0], #0x20
	stp	q6,q7, [x0], #0x20
	stp	q8,q9, [x0], #0x20
	stp	q10,q11, [x0], #0x20
	stp	q12,q13, [x0], #0x20
	stp	q14,q15, [x0], #0x20
	stp	q16,q17, [x0], #0x20
	stp	q18,q19, [x0], #0x20
	stp	q20,q21, [x0], #0x20
	stp	q22,q23, [x0], #0x20
	stp	q24,q25, [x0], #0x20
	stp	q26,q27, [x0], #0x20
	stp	q28,q29, [x0], #0x20
	stp	q30,q31, [x0], #0x20
	mrs	x2, FPCR
	mrs	x3, FPSR
	stp	x2, x3, [x0], #0x10

/* Save current address of floating point context array to FPUContextBase */
	str	x0, [x1]
.endm

.macro macro_restorefloatregister
/* Restore the address of floating point context array from FPUContextBase */
	ldr	x1,=FPUContextBase
	ldr	x0, [x1]

/* Restore all the floating point register from the array */
	ldp	x2, x3, [x0,#-0x10]!
	msr	FPCR, x2
	msr	FPSR, x3
	ldp	q30,q31, [x0,#-0x20]!
	ldp	q28,q29, [x0,#-0x20]!
	ldp	q26,q27, [x0,#-0x20]!
	ldp	q24,q25, [x0,#-0x20]!
	ldp	q22,q23, [x0,#-0x20]!
	ldp	q20,q21, [x0,#-0x20]!
	ldp	q18,q19, [x0,#-0x20]!
	ldp	q16,q17, [x0,#-0x20]!
	ldp	q14,q15, [x0,#-0x20]!
	ldp	q12,q13, [x0,#-0x20]!
	ldp	q10,q11, [x0,#-0x20]!
	ldp	q8,q9, [x0,#-0x20]!
	ldp	q6,q7, [x0,#-0x20]!
	ldp	q4,q5, [x0,#-0x20]!
	ldp	q2,q3, [x0,#-0x20]!
	ldp	q0,q1, [x0,#-0x20]!

/* Save current address of floating point context array to FPUContextBase */
	str	x0, [x1]
.endm

.macro macro_exception_return
	eret
.endm

/*Each entry is 128 bytes long */
.org 0

.section .vectors, "a"

_vector_table:
.set	VBAR, _vector_table

.org VBAR
	b	_boot

.org (VBAR + 0x200)
	b	SynchronousInterruptHandler

.org (VBAR + 0x280)
	b	IRQInterruptHandler

.org (VBAR + 0x300)
	b	FIQInterruptHandler

.org (VBAR + 0x380)
	b	SErrorInterruptHandler

/* The exception handler for the synchronous
*  exception from a lower EL or same EL
*/
SynchronousInterruptHandler:
	macro_saveregister

	/* Check if the Synchronous abort is occurred due to floating point access. */
	.if (EL3 == 1)
		mrs	x0, ESR_EL3
	.endif
		and	x0, x0, #(0x3F << 26)
		mov	x1, #(0x7 << 26)
		cmp	x0, x1
	/* If exception is not due to floating point access go to synchronous handler */
		bne	subroutine_synchronoushandler

	/*
 	* If exception occurred due to floating point access, Enable the floating point
 	* access i.e. do not trap floating point instruction
 	*/
 	.if (EL3 == 1)
		mrs	x1,CPTR_EL3
		bic	x1, x1, #(0x1<<10)
		msr	CPTR_EL3, x1
	.endif
		isb

	/* If the floating point access was previously enabled, store FPU context
	 * registers(storefloat).
	 */
	ldr	x0, =FPUStatus
	ldrb	w1,[x0]
	cbnz	w1, subroutine_storefloat
	/*
 	* If the floating point access was not enabled previously, save the status of
 	* floating point accessibility i.e. enabled and store floating point context
 	* array address(FPUContext) to FPUContextBase.
 	*/
	mov	w1, #0x1
	strb	w1, [x0]
	ldr	x0, =FPUContext
	ldr	x1, =FPUContextBase
	str	x0,[x1]
	b	subroutine_restorecontext


/* The exception handler for the IRQ exception
*  exception from a lower EL or same EL
*/
IRQInterruptHandler:
	macro_saveregister
/* Save the status of SPSR, ELR and CPTR to stack */
 .if (EL3 == 1)
	mrs 	x0, CPTR_EL3
	mrs 	x1, ELR_EL3
	mrs	x2, SPSR_EL3
.endif
	stp	x0, x1, [sp,#-0x10]!
	str	x2, [sp,#-0x10]!

/* Trap floating point access */
 .if (EL3 == 1)
	mrs	x1,CPTR_EL3
	orr	x1, x1, #(0x1<<10)
	msr	CPTR_EL3, x1
.endif
	isb

	bl	IRQInterrupt
/*
 * If floating point access is enabled during interrupt handling,
 * restore floating point registers.
 */

.if (EL3 == 1)
	mrs	x0, CPTR_EL3
	ands	x0, x0, #(0x1<<10)
	bne	RestorePrevState
.endif
	macro_restorefloatregister

	/* Restore the status of SPSR, ELR and CPTR from stack */
	RestorePrevState:
		ldr	x2,[sp],0x10
		ldp	x0, x1, [sp],0x10
 	.if (EL3 == 1)
		msr	CPTR_EL3, x0
		msr	ELR_EL3, x1
		msr	SPSR_EL3, x2
	.endif

	macro_restoreregister
	macro_exception_return



/* The exception handler for the FIQ
*  exception from a lower EL or same EL
*/
FIQInterruptHandler:
	macro_saveregister

	/* Save the status of SPSR, ELR and CPTR to stack */
	.if (EL3 == 1)
		mrs 	x0, CPTR_EL3
		mrs 	x1, ELR_EL3
		mrs	x2, SPSR_EL3
	.endif
		stp	x0, x1, [sp,#-0x10]!
		str	x2, [sp,#-0x10]!

	/* Trap floating point access */
	.if (EL3 == 1)
		mrs	x1,CPTR_EL3
		orr	x1, x1, #(0x1<<10)
		msr	CPTR_EL3, x1
		isb
		bl	FIQInterrupt
	.endif

	/*
	 * If floating point access is enabled during interrupt handling,
 	* restore floating point registers.
 	*/
	.if (EL3 == 1)
		mrs	x0, CPTR_EL3
		ands	x0, x0, #(0x1<<10)
		bne	subroutine_restorePrevStatefiq
	.endif
	macro_restorefloatregister

/* The exception handler for the system error
*  exception from a lower EL or same EL
*/
SErrorInterruptHandler:
	macro_saveregister
	bl      SErrorInterrupt
	macro_restoreregister
	macro_exception_return

/* store floating point context */
subroutine_storefloat:
	macro_savefloatregister
	b	subroutine_restorecontext

/* synchronous handler */
subroutine_synchronoushandler:
	bl	SynchronousInterrupt

subroutine_restorecontext:
	macro_restoreregister
	macro_exception_return

/* Restore the status of SPSR, ELR and CPTR from stack */
subroutine_restorePrevStatefiq:
	ldr	x2,[sp],0x10
	ldp	x0, x1, [sp],0x10
.if (EL3 == 1)
	msr	CPTR_EL3, x0
	msr	ELR_EL3, x1
	msr	SPSR_EL3, x2
.endif
	macro_restoreregister
	macro_exception_return


.align 8
/* Array to store floating point registers */
FPUContext: .skip FPUContextSize

/* Stores address for floating point context array */
FPUContextBase: .skip 8

FPUStatus: .skip 1

.end
