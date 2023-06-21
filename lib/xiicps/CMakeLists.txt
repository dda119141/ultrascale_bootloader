cmake_minimum_required(VERSION 3.14)

add_library(iicps STATIC)

target_sources(iicps PUBLIC
	xiicps.c
	xiicps_g.c
	xiicps_hw.c
	xiicps_intr.c
	xiicps_xfer.c
	xiicps_master.c
	xiicps_slave.c
	xiicps_options.c
	xiicps_sinit.c)

target_include_directories(iicps PRIVATE 
	"../uartps"
	"../common")


