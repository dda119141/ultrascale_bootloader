cmake_minimum_required(VERSION 3.14)

add_library(xil STATIC)

target_sources(xil PUBLIC
	boot.S
	asm_vectors.S
	invalidate_caches.S
	xfsbl_translation_table_a53_64.S
	xil-crt0.S
	initialise_monitor_handles.c
	xil_assert.c
	xil_cache.c
	xil_exception.c
	xil_mem.c
	#	xil_mmu.c
	xil_printf.c
	xil_sleepcommon.c
	xil_sleeptimer.c
	xil_smc.c
	xil_util.c
	xplatform_info.c
	sleep.c
	vectors.c
#	sbrk.c
	time.c
	lseek.c
	xtime_l.c
	close.c
	read.c
	_exit.c
	_open.c
	_sbrk.c
	outbyte.c
	inbyte.c
	write.c
	)

set_target_properties(xil PROPERTIES
 LINK_FLAGS "rc"
 ) 


target_include_directories(xil PRIVATE 
	"../uartps"
	"../common")

