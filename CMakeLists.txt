cmake_minimum_required(VERSION 3.14)

project(fsboot_a53_zc102.elf LANGUAGES C ASM)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(${PROJECT_NAME}) 

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

target_link_options(${PROJECT_NAME} PRIVATE 
	-T${CMAKE_SOURCE_DIR}/lscript.ld 
		)

set_target_properties(${PROJECT_NAME} PROPERTIES
 LINK_DEPENDS "${LINKER_SCRIPT}"
 LINK_FLAGS "-fmessage-length=0 "
 ) 

add_subdirectory(src)

#include(clang_tidy)
#AddClangTidy(${PROJECT_NAME})

