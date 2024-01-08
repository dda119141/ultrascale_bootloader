cmake_minimum_required(VERSION 3.14)

add_library(xipipsu STATIC)

target_sources(xipipsu PUBLIC
	xipipsu.c
	xipipsu_g.c
	xipipsu_buf.c
	xipipsu_sinit.c)


set_target_properties(xipipsu PROPERTIES
 LINK_FLAGS "rc"
 ) 

target_include_directories(xipipsu PRIVATE 
	"../uartps"
	"../common")


