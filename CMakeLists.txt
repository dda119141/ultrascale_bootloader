cmake_minimum_required(VERSION 3.14)

project(fsboot_a53_zc102.elf LANGUAGES C ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

#set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME} 
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
	pm_clock.c
	pm_callbacks.c
	pm_cfg_obj.c
	pm_client.c
	pm_api_sys.c
	generated/psu_init.c
	)

target_compile_options(${PROJECT_NAME} PRIVATE
			-Os -MMD -MP
			-DARMA53_64
			-D__aarch64__
		 	-Wall -Werror -g 
			-ffunction-sections
			-fdata-sections
#			-flto -ffat-lto-objects
#			-nostdlib
			 )

target_include_directories(${PROJECT_NAME} PRIVATE 
	"lib/uartps"
	"lib/qspipsu"
	"lib/ipipsu"
	"lib/xiicps"
	"generated"
	"lib/common")

target_link_libraries(${PROJECT_NAME} PRIVATE 
	"xil"
	"uartps"
	"common"
	"qspipsu"
	"xipipsu"
	"iicps")

target_link_options(${PROJECT_NAME} PRIVATE 
	-T${CMAKE_SOURCE_DIR}/lscript.ld 
		)

set_target_properties(${PROJECT_NAME} PROPERTIES
 LINK_DEPENDS "${LINKER_SCRIPT}"
 LINK_FLAGS "-fmessage-length=0 "
 ) 

add_subdirectory(lib/common)
add_subdirectory(lib/uartps)
add_subdirectory(lib/bootup)
add_subdirectory(lib/qspipsu)
add_subdirectory(lib/xiicps)
add_subdirectory(lib/ipipsu)

include(clang_tidy)
AddClangTidy(${PROJECT_NAME})

