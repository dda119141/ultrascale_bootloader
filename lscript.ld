/******************************************************************************
*
******************************************************************************/

/*******************************************************************/
/*******************************************************************/
_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x2000;

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
   *(.rodata)
   *(.data)
   *(.bss)
   *(.stack)
} > psu_ocm_ram_0_S_AXI_BASEADDR

.rodata (ALIGN(64)): {
   __rodata_start = .;
   *(.rodata)
   *(.rodata.*)
   __rodata_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.data (ALIGN(64)): {
   __data_start = .;
   *(.data)
   __data_end = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.bss (NOLOAD) : {
   . = ALIGN(64);
   __bss_start__ = .;
   *(.bss)
   *(.bss.*)
   . = ALIGN(64);
   __bss_end__ = .;
/*   __bss_size = __bss_end - __bss_start; */
} > psu_ocm_ram_0_S_AXI_BASEADDR

/* Generate Stack definitions */
.stack (NOLOAD) : {
   . = ALIGN(64);
   _el3_stack_end = .;
   . += _STACK_SIZE;
   __el3_stack = .;
} > psu_ocm_ram_0_S_AXI_BASEADDR

.handoff_params (NOLOAD) : {
   . = ALIGN(512);
   *(.handoff_params)
} > psu_ocm_ram_1_S_AXI_BASEADDR

.heap : {
   _heap_start = .;
   . += 0x0;
   _heap_end = .;
   . = ALIGN(4);
} > psu_ocm_ram_1_S_AXI_BASEADDR


/*
.bitstream_buffer (NOLOAD) : {
	. = ALIGN(32);
	*(.bitstream_buffer)
} > psu_ocm_ram_2_S_AXI_BASEADDR
*/
_end = .;
}
