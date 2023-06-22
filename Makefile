#/******************************************************************************
#* SPDX-License-Identifier: MIT
#******************************************************************************/


PROC ?= a53
A53_STATE := 64
CC = $(CROSS_COMPILE)gcc $(KCFLAGS)
AR = $(CROSS_COMPILE)ar

linker_script := -T$(CURDIR)/lscript.ld
BOARD	:= zcu102
LIBS = libxil.a
export BUILD_OUTPUT = $(CURDIR)/dist
EXEC := $(BUILD_OUTPUT)/fsbl_a53_zc102.elf

source_files := $(wildcard *.c)
source_files += generated/psu_init.c
assembler_sources := $(wildcard *.S)

includes := $(wildcard *.h)
OBJS := $(patsubst %.c, $(BUILD_OUTPUT)/%.o, $(source_files))
OBJS += $(patsubst %.S, $(BUILD_OUTPUT)/%.o, $(assembler_sources))
OBJS_D := $(patsubst %.c, $(BUILD_OUTPUT)/%.d, $(source_files))
OBJS_SD := $(patsubst %.S, $(BUILD_OUTPUT)/%.d, $(assembler_sources))


COMPILE_DEFINES = -DARMA53_$(A53_STATE)
CFLAGS = $(COMPILE_DEFINES)
CFLAGS += -Os
CFLAGS += -flto -ffat-lto-objects 
CFLAGS += -c -g -MMD -MP -Wall -Werror -fmessage-length=0 

sub_component := lib
sub_component += lib/uartps
sub_component += lib/bootup
sub_component += lib/xiicps
sub_component += lib/qspipsu 
#sub_component += lib/xilffs
#sub_component += lib/sdps 
#sub_component += lib/xilpm/common

INCLUDEPATH += -I$(CURDIR) 
INCLUDEPATH += -I$(CURDIR)/generated 
INCLUDEPATH += -I$(CURDIR)/lib/common 
INCLUDEPATH += $(addprefix -I,$(sub_component))

DUMP    :=      $(CROSS_COMPILE)objdump -xSD
LDFLAGS := -DARMA53_$(A53_STATE) -MMD -MP -Wall -fmessage-length=0 -Os -flto -ffat-lto-objects
LDFLAGS += -Wl,--start-group,-luartps,-lqspipsu,-lxiicps,-lxil,-lgcc,-lc,--end-group
LDFLAGS += $(addprefix -L$(BUILD_OUTPUT)/,$(sub_component))


.PHONY: all
all: $(EXEC)

PHONY += $(EXEC)
$(EXEC): $(LIBS) $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(linker_script)
	$(DUMP) $(EXEC)  > dump

PHONY += $(LIBS)
$(LIBS): 
	echo "Compiling bsp"
	for d in $(sub_component); \
		do \
		$(MAKE) --directory=$$d; \
		done

$(OBJS): $(source_files) $(assembler_sources) | $(BUILD_OUTPUT)
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDEPATH)

$(BUILD_OUTPUT): mk_gen
	[ -d $@ ] || mkdir -p $@

mk_gen:
	[ -d $(BUILD_OUTPUT)/generated ] || mkdir $(BUILD_OUTPUT)/generated

PHONY += clean
clean:
	for d in $(sub_component); \
		do \
		$(MAKE) --directory=$$d clean; \
		done

	rm -rf $(OBJS) $(OBJS_D) $(OBJS_SD) $(BUILD_OUTPUT) dump
