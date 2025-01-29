cmake_minimum_required(VERSION 3.14)

target_sources(${PROJECT_NAME} PUBLIC 
	xfsbl_exit.S
	xfsbl_board.c
	xfsbl_hooks.c
	xfsbl_image_header.c
	xfsbl_misc_drivers.c
	xfsbl_partition_load.c
	xfsbl_initialization.c
	xfsbl_handoff.c
	xfsbl_ddr_init.c
	xfsbl_qspi.c
	xfsbl_main.c
	xfsbl_misc.c
	)

