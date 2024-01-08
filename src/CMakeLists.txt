cmake_minimum_required(VERSION 3.14)

target_include_directories(${PROJECT_NAME} PRIVATE 
	"lib/uartps"
	"lib/qspipsu"
	"lib/ipipsu"
	"lib/xiicps"
	"generated"
	"pm"
	"lib/common")

target_link_libraries(${PROJECT_NAME} PRIVATE 
	"xil"
	"uartps"
	"common"
	"qspipsu"
	"xipipsu"
	"iicps")

add_subdirectory(main)
add_subdirectory(pm)
add_subdirectory(generated)
add_subdirectory(lib/common)
add_subdirectory(lib/uartps)
add_subdirectory(lib/bootup)
add_subdirectory(lib/qspipsu)
add_subdirectory(lib/xiicps)
add_subdirectory(lib/ipipsu)


