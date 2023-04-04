/******************************************************************************
*
* Copyright (c) 2015 - 2021 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*******************************************************************/
/*                                                                 */
/* Description : FSBL A53 Linker Script                            */
/*                                                                 */
/*******************************************************************/
_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x2000;
_HEAP_SIZE = DEFINED(_HEAP_SIZE) ? _HEAP_SIZE : 1024;

/* Define Memories in the system */

MEMORY
{
   psu_ocm_ram_0_S_AXI_BASEADDR : ORIGIN = 0xFFFC0000, LENGTH = 0x00029E00
   psu_ocm_ram_1_S_AXI_BASEADDR : ORIGIN = 0xFFFE9E00, LENGTH = 0x00000200
   psu_ocm_ram_2_S_AXI_BASEADDR : ORIGIN = 0xFFFF0040, LENGTH = 0x0000FDC0

}

/* Specify the default entry point to the program */

ENTRY(_vector_table)

/* Define the sections, and where they are mapped in memory */

SECTIONS
{
.text : {
   KEEP (*(.vectors))
   *(.boot)
   *(.text)
   *(.text.*)
   *(.gnu.linkonce.t.*)
   *(.plt)
   *(.gnu_warning)
   *(.gcc_execpt_table)
   *(.glue_7)
   *(.glue_7t)
   *(.ARM.extab)
   *(.gnu.linkonce.armextab.*)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.note.gnu.build-id : {
   KEEP (*(.note.gnu.build-id))
} > psu_ocm_ram_0_S_AXI_BASEADDR

.init (ALIGN(64)): {
   KEEP (*(.init))
} > psu_ocm_ram_0_S_AXI_BASEADDR

.fini (ALIGN(64)): {
   KEEP (*(.fini))
} > psu_ocm_ram_0_S_AXI_BASEADDR

.interp : {
   __interp_start = .;
   KEEP (*(.interp))
   __interp_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.note-ABI-tag : {
   __note-ABI-tag_start = .;
   KEEP (*(.note-ABI-tag))
   __note-ABI-tag_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.rodata (ALIGN(64)): {
   __rodata_start = .;
   *(.rodata)
   *(.rodata.*)
   *(.gnu.linkonce.r.*)
   __rodata_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.rodata1 (ALIGN(64)): {
   __rodata1_start = .;
   *(.rodata1)
   *(.rodata1.*)
   __rodata1_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.sys_cfg_data (ALIGN(64)): {
   *(.sys_cfg_data)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.eh_frame : {
  *(.eh_frame)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.eh_framehdr : {
   __eh_framehdr_start = .;
   *(.eh_framehdr)
   __eh_framehdr_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.gcc_except_table : {
   *(.gcc_except_table)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.data (ALIGN(64)): {
   __data_start = .;
   *(.data)
   *(.data.*)
   *(.gnu.linkonce.d.*)
   *(.jcr)
   __data_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.got1 : {
   *(.got1)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.got2 : {
   *(.got2)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.fixup : {
   __fixup_start = .;
   *(.fixup)
   __fixup_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.ARM.exidx : {
   __exidx_start = .;
   *(.ARM.exidx*)
   *(.gnu.linkonce.armexidix.*.*)
   __exidx_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.preinit_array (ALIGN(64)): {
   __preinit_array_start = .;
   KEEP (*(SORT(.preinit_array.*)))
   KEEP (*(.preinit_array))
   __preinit_array_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR
/*
.init_array (ALIGN(64)): {
   __init_array_start = .;
   KEEP (*(SORT(.init_array.*)))
   KEEP (*(.init_array))
   __init_array_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR


.fini_array (ALIGN(64)): {
   __fini_array_start = .;
   KEEP (*(SORT(.fini_array.*)))
   KEEP (*(.fini_array))
   __fini_array_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR
*/
.ARM.attributes : {
   __ARM.attributes_start = .;
   *(.ARM.attributes)
   __ARM.attributes_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.sbss (NOLOAD) : {
   __sbss_start = .;
 . = ALIGN(64);
   *(.sbss)
   *(.sbss.*)
   *(.gnu.linkonce.sb.*)
    . = ALIGN(64);
   __sbss_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.bss (NOLOAD) : {
   . = ALIGN(64);
   __bss_start__ = .;
   *(.bss)
   *(.bss.*)
   *(.gnu.linkonce.b.*)
   *(COMMON)
   . = ALIGN(64);
   __bss_end__ = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR



/* Generate Stack and Heap definitions */

.heap (NOLOAD) : {
   . = ALIGN(64);
   _heap = .;
   HeapBase = .;
   _heap_start = .;
   . += _HEAP_SIZE;
   _heap_end = .;
   HeapLimit = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.stack (NOLOAD) : {
   . = ALIGN(64);
   _el3_stack_end = .;
   . += _STACK_SIZE;
   __el3_stack = .;
   __el2_stack = .;
   __el1_stack = .;
   __el0_stack = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.handoff_params (NOLOAD) : {
   . = ALIGN(512);
   *(.handoff_params)
} > psu_ocm_ram_1_S_AXI_BASEADDR

/*
.bitstream_buffer (NOLOAD) : {
	. = ALIGN(32);
	*(.bitstream_buffer)
} > psu_ocm_ram_2_S_AXI_BASEADDR
*/
_end = .;
}
