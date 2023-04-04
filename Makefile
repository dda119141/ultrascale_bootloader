#/******************************************************************************
#* Copyright (c) 2015 - 2020 Xilinx, Inc.  All rights reserved.
#* SPDX-License-Identifier: MIT
#******************************************************************************/


PROC ?= a53
A53_STATE := 64
CC = $(CROSS_COMPILE)gcc $(KCFLAGS)
AR = $(CROSS_COMPILE)ar

linker_script := -Tlscript.ld
BOARD	:= zcu102
EXEC := fsboot_a53.elf
LIBS = libxil.a

source_files := $(wildcard *.c)
source_files += generated/psu_init.c
assembler_sources := $(wildcard *.S)

includes := $(wildcard *.h)
OBJS := $(patsubst %.c, %.o, $(source_files))
OBJS += $(patsubst %.S, %.o, $(assembler_sources))
OBJS_D := $(patsubst %.c, %.d, $(source_files))
OBJS_SD := $(patsubst %.S, %.d, $(assembler_sources))

CFLAGS = -c -MMD -MP -Wall -fmessage-length=0 

sub_component := lib
sub_component += lib/uartps
sub_component += lib/bootup
sub_component += lib/xiicps
#sub_component += lib/xilffs
#sub_component += lib/sdps 
#sub_component += lib/xilpm/common

EXEC := fsbl_a53_zc102.elf

INCLUDEPATH += -I$(CURDIR) 
INCLUDEPATH += -I$(CURDIR)/generated 
INCLUDEPATH += -I$(CURDIR)/lib/include 
INCLUDEPATH += $(addprefix -I,$(sub_component))

DUMP    :=      $(CROSS_COMPILE)objdump -xSD
ECFLAGS = -DARMA53_$(A53_STATE) -Os -flto -ffat-lto-objects
LDFLAGS := -DARMA53_$(A53_STATE) -MMD -MP -Wall -fmessage-length=0 -Os -flto -ffat-lto-objects
LDFLAGS += -Wl,--start-group,-luartps,-lxiicps,-lxil,-lgcc,-lc,--end-group
LDFLAGS += $(addprefix -L,$(sub_component))


.PHONY: all
all: $(EXEC)

PHONY += $(EXEC)
$(EXEC): $(LIBS) $(OBJS) $(includes)
	$(CC) -o $@ $(OBJS) $(CC_FLAGS) $(LDFLAGS) $(linker_script)
	$(DUMP) $(EXEC)  > dump

PHONY += $(LIBS)
$(LIBS): 
	echo "Compiling bsp"
	for d in $(sub_component); \
		do \
		$(MAKE) --directory=$$d; \
		done

%.o:%.c
	$(CC) $(CC_FLAGS) $(CFLAGS) $(ECFLAGS) -c $< -o $@ $(INCLUDEPATH)

%.o:%.S
	$(CC) $(CC_FLAGS) $(CFLAGS) $(ECFLAGS) -c $< -o $@ $(INCLUDEPATH)

%.o:%.s
	$(CC) $(CC_FLAGS) $(CFLAGS) $(ECFLAGS) -c $< -o $@ $(INCLUDEPATH)

PHONY += clean
clean:
	for d in $(sub_component); \
		do \
		$(MAKE) --directory=$$d clean; \
		done

	rm -rf $(OBJS) $(OBJS_D) $(OBJS_SD) *.elf dump
