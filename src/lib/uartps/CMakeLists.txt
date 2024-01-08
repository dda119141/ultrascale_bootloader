cmake_minimum_required(VERSION 3.14)

add_library(uartps STATIC)

target_sources(uartps PUBLIC
	xuartps.c
	xuartps_g.c
	xuartps_hw.c
	xuartps_intr.c
	xuartps_options.c
	xuartps_sinit.c
	)

target_include_directories(uartps PUBLIC 
	"../common")
