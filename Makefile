#/******************************************************************************
#* SPDX-License-Identifier: MIT
#******************************************************************************/


PROC ?= a53
A53_STATE := 64
CC = $(CROSS_COMPILE)gcc $(KCFLAGS)
AR = $(CROSS_COMPILE)ar

LOCAL_DIR = $(CURDIR)
linker_script := -T$(LOCAL_DIR)/lscript.ld
BOARD	:= zcu102
LIBS = libxil.a
export BUILD_OUTPUT = $(LOCAL_DIR)/dist
EXEC := $(BUILD_OUTPUT)/fsbl_a53_zc102.elf

source_files_ = $(wildcard *.c)
source_files += generated/psu_init.c
source_files = $(patsubst %.c, $(LOCAL_DIR)/%.c, $(source_files_))
assembler_sources_ := $(wildcard *.S)
assembler_source = $(patsubst %.c, $(LOCAL_DIR)/%.c, $(assembler_sources__))

includes := $(wildcard *.h)
C_SOURCE_OBJECTS = $(patsubst %.c, $(BUILD_OUTPUT)/%.o, $(source_files_))
ASSEMBLER_SOURCE_OBJS = $(patsubst %.S, $(BUILD_OUTPUT)/%.o, $(assembler_sources_))
OBJS_D := $(patsubst %.c, $(BUILD_OUTPUT)/%.d, $(source_files_))
OBJS_SD := $(patsubst %.S, $(BUILD_OUTPUT)/%.d, $(assembler_sources_))


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

INCLUDEPATH += -I$(LOCAL_DIR) 
INCLUDEPATH += -I$(LOCAL_DIR)/generated 
INCLUDEPATH += -I$(LOCAL_DIR)/lib/common 
INCLUDEPATH += $(addprefix -I,$(sub_component))

DUMP    :=      $(CROSS_COMPILE)objdump -xSD
LDFLAGS := -DARMA53_$(A53_STATE) -MMD -MP -Wall -fmessage-length=0 -Os -flto -ffat-lto-objects
LDFLAGS += -Wl,--start-group,-luartps,-lqspipsu,-lxiicps,-lxil,-lgcc,-lc,--end-group
LDFLAGS += $(addprefix -L$(BUILD_OUTPUT)/,$(sub_component))


.PHONY: all
all: $(EXEC)

PHONY += $(EXEC)
$(EXEC): $(LIBS) $(C_SOURCE_OBJECTS) $(ASSEMBLER_SOURCE_OBJS)
	$(CC) -o $@ $(C_SOURCE_OBJECTS) $(ASSEMBLER_SOURCE_OBJS) $(LDFLAGS) $(linker_script)
	$(DUMP) $(EXEC)  > dump

PHONY += $(LIBS)
$(LIBS): 
	echo "Compiling bsp"
	for d in $(sub_component); \
		do \
		$(MAKE) --directory=$$d; \
		done

$(C_SOURCE_OBJECTS): $(source_files) | $(BUILD_OUTPUT)
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDEPATH)

$(ASSEMBLER_SOURCE_OBJS): $(assembler_sources)
	$(CC) $(CFLAGS) -c $< -o $@ $(INCLUDEPATH)

$(BUILD_OUTPUT): mk_gen
	[ -d $@ ] || mkdir -p $@

mk_gen:
	[ -d $(BUILD_OUTPUT)/generated ] || mkdir $(BUILD_OUTPUT)/generated

dop:
	echo $(source_files)
	echo $(C_SOURCE_OBJECTS)

PHONY += clean
clean:
	for d in $(sub_component); \
		do \
		$(MAKE) --directory=$$d clean; \
		done

	rm -rf $(C_SOURCE_OBJECTS) $(ASSEMBLER_SOURCE_OBJS) $(OBJS_D) $(OBJS_SD) $(BUILD_OUTPUT) dump
