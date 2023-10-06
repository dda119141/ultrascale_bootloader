cmake_minimum_required(VERSION 3.14)

project(libboot LANGUAGES C VERSION 1.0)

target_compile_options(libboot PRIVATE
		 	 -Wall -Werror -g -ftest-coverage -pthread -fprofile-arcs
			 )

add_subdirectory(common)
add_subdirectory(uartps)
add_subdirectory(bootup)
add_subdirectory(qspipsu)
add_subdirectory(ipipsu)
