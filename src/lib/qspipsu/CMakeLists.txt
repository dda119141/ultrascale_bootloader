cmake_minimum_required(VERSION 3.14)

add_library(qspipsu STATIC)

target_sources(qspipsu PUBLIC
	xqspipsu.c
	xqspipsu_control.c
	xqspipsu_g.c
	xqspipsu_hw.c
	xqspipsu_options.c
	xqspipsu_sinit.c)


set_target_properties(qspipsu PROPERTIES
 LINK_FLAGS "rc"
 ) 

target_include_directories(qspipsu PRIVATE 
	"../uartps"
	"../common")


