###############################################################################
# Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
# SPDX-License-Identifier: MIT
###############################################################################
DRIVER_LIB_VERSION = v1.0

library ?= libxil.a

CC = $(CROSS_COMPILE)gcc $(KCFLAGS)
AR = $(CROSS_COMPILE)ar

LOCAL_DIR ?= $(CURDIR)

INCLUDEDIR = $(LOCAL_DIR)
INCLUDEDIR += $(LOCAL_DIR)/../common
INCLUDEDIR += $(LOCAL_DIR)/../../

INCLUDEPATH += $(addprefix -I ,$(INCLUDEDIR))

OUTPUT = $(BUILD_OUTPUT)/lib/$(DIR_NAME)
OUTLIB = $(OUTPUT)/$(library)

assembler_sources_rel := $(wildcard *.S)
assembler_sources = $(patsubst %.S, $(LOCAL_DIR)/%.S, $(assembler_sources_rel))
sources_rel := $(wildcard *.c)
sources = $(patsubst %.c, $(LOCAL_DIR)/%.c, $(sources_rel))

objects = $(patsubst %.c, $(OUTPUT)/%.o, $(sources_rel))
assembler_objects = $(patsubst %.S, $(OUTPUT)/%.o, $(assembler_sources_rel))
all_objects = $(objects)
all_objects += $(assembler_objects)

EXTRA_ARCHIVE_FLAGS=rc

.PHONY: library
library: $(OUTLIB)

$(OUTLIB): $(objects) $(assembler_objects)
	$(AR) $(EXTRA_ARCHIVE_FLAGS) $@ $^

$(assembler_objects): $(assembler_sources) | $(OUTPUT)
	$(CC) $(INCLUDEPATH) -c $< -o $@

$(objects): $(sources) | $(OUTPUT)
	$(CC) $(INCLUDEPATH) -c $< -o $@

$(OUTPUT):
	[ -d $@ ] || mkdir -p $@;

PHONY += clean
clean:
	rm -rf ${all_objects}
	rm -rf ${library}


