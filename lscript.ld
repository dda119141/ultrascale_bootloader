/******************************************************************************
*
******************************************************************************/

/*******************************************************************/
/*******************************************************************/
_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x2000;

/* Define Memories in the system */
MEMORY
{
   psu_ocm_ram_0_S_AXI_BASEADDR (rx) : ORIGIN = 0xFFFC0000, LENGTH = 0x00032000
   psu_ocm_ram_1_S_AXI_BASEADDR (rx): ORIGIN = 0xFFFF2000, LENGTH = 0x00000200
   psu_ocm_ram_2_S_AXI_BASEADDR (rwx): ORIGIN = 0xFFFC0200, LENGTH = 0x0003FE00
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
   *(.rodata*)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.data (ALIGN(64)): {
   __data_start = .;
   *(.data)
   __data_end = .;
} > psu_ocm_ram_2_S_AXI_BASEADDR

.bss (NOLOAD) : {
   . = ALIGN(64);
   __bss_start__ = .;
   *(.bss)
   *(.bss.*)
   . = ALIGN(64);
   __bss_end__ = .;
/*   __bss_size = __bss_end - __bss_start; */
} > psu_ocm_ram_2_S_AXI_BASEADDR

/* Generate Stack definitions */
.stack (NOLOAD) : {
   . = ALIGN(64);
   _el3_stack_end = .;
   . += _STACK_SIZE;
   . = ALIGN(64);
   __el3_stack = .;
} > psu_ocm_ram_2_S_AXI_BASEADDR

.handoff_params (NOLOAD) : {
   . = ALIGN(512);
   *(.handoff_params)
} > psu_ocm_ram_1_S_AXI_BASEADDR

.heap : {
   _heap_start = .;
   . += 0x0;
   _heap_end = .;
   . = ALIGN(4);
} > psu_ocm_ram_2_S_AXI_BASEADDR


/*
.bitstream_buffer (NOLOAD) : {
	. = ALIGN(32);
	*(.bitstream_buffer)
} > psu_ocm_ram_2_S_AXI_BASEADDR
*/
_end = .;
}
