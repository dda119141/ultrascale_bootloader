cmake_minimum_required(VERSION 3.14)

add_library(common INTERFACE)

target_sources(common INTERFACE
	xil_assert.h
	xstatus.h
	xil_cache.h
	sleep.h
	xil_exception.h
	bspconfig.h
	xil_printf.h
	xil_mem.h
	xil_util.h
	xil_types.h
	xparameters.h
	xparameters_ps.h
	xreg_cortexa53.h
	)

