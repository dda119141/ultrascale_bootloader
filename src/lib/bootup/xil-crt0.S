/******************************************************************************
* Copyright (C) 2014 - 2022 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/
/*****************************************************************************/
/**
* @file xil-crt0.S
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- ---------------------------------------------------
* 5.00	pkp  05/21/14 Initial version
* 5.04  pkp  12/18/15 Initialized global constructor for C++ applications
* 5.04	pkp  01/05/16 Set the reset vector register RVBAR equivalent to
*		      vector table base address
* 6.02  pkp  01/22/17 Added support for EL1 non-secure
* 6.6   srm  10/18/17 Added timer configuration using XTime_StartTTCTimer API.
*		      Now the TTC instance as specified by the user will be
*	              started.
* 6.6   mus  01/29/18 Initialized the xen PV console for Cortexa53 64 bit
*                     EL1 NS BSP.
* 7.2   sd   02/23/20 Clock Init is called
* 7.2   sd   02/23/20 Clock code added under XCLOCKING flag
* 7.7   mus  01/06/22 Added call to Xil_SetTlbAttributes to set correct
*                     attributes for GIC in case of Xen domU guest application.
*                     It fixes CR#974078.
* 8.0   mus  07/06/21 Added support for VERSAL NET
* 8.0   mus  06/27/22 Enabled PMU counter.
* </pre>
*
* @note
*
* None.
*
******************************************************************************/
#include "xparameters.h"
#include "bspconfig.h"
	.file	"xil-crt0.S"


.globl	_startup

_startup:
	mov	x0, #0

/*  clear bss */
    ldr     x5, =__bss_start__
	mov     x9, #0
    ldr     x9, =__bss_end__
	sub 	x6, x9, x5 /* calculate bss size */
/*    ldr     w6, =__bss_size */

1:	cbz     w6, 2f
	str     xzr, [x5], #8
	sub     w6, w6, #1
	cbnz    w6, 1b

	/* make sure argc and argv are valid */
2:	mov	x0, #0
	mov	x1, #0

	bl	main			/* Jump to main C code */
