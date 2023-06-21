cmake_minimum_required(VERSION 3.14)

project(fsboot_a53 LANGUAGES C ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(fsboot_a53 
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
	generated/psu_init.c
	)

add_compile_options(-fprofile-arcs -ftest-coverage)
#			-flto -ffat-lto-objects

target_compile_options(fsboot_a53 PRIVATE
			-DARMA53_64
		 	-Wall -Werror -g -ftest-coverage -fprofile-arcs
			#-flto -ffat-lto-objects
			 )

target_include_directories(fsboot_a53 PRIVATE 
	"lib/uartps"
	"lib/qspipsu"
	"lib/xiicps"
	"generated"
	"lib/common")

target_link_libraries(fsboot_a53 PRIVATE 
	"xil"
	"uartps"
	"common"
	"qspipsu"
	"iicps")

target_link_options(fsboot_a53 PRIVATE 
	-T${CMAKE_SOURCE_DIR}/lscript.ld 
		)

set_target_properties(fsboot_a53 PROPERTIES
 LINK_DEPENDS "${LINKER_SCRIPT}"
 LINK_FLAGS "-DARMA53_64 -MMD -MP -Wall -fmessage-length=0 -Os -flto -ffat-lto-objects"
 ) 

add_subdirectory(lib/common)
add_subdirectory(lib/uartps)
add_subdirectory(lib/bootup)
add_subdirectory(lib/qspipsu)
add_subdirectory(lib/xiicps)

include(clang_tidy)
AddClangTidy(fsboot_a53)

