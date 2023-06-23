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

assembler_sources := $(wildcard *.S)
sources := $(wildcard *.c)

vpath %.c $(OUTPUT)
vpath %.s $(OUTPUT)

objects = $(patsubst %.c, $(OUTPUT)/%.o, $(sources))
assembler_objects = $(patsubst %.S, $(OUTPUT)/%.o, $(assembler_sources))
all_objects = $(objects)
all_objects += $(assembler_objects)

EXTRA_ARCHIVE_FLAGS=rc

.PHONY: library
library: $(OUTLIB)

$(OUTLIB): $(all_objects) 
	$(AR) $(EXTRA_ARCHIVE_FLAGS) $@ $^

#$(all_objects): $(assembler_sources) $(sources) | $(OUTPUT)
$(all_objects): %.S $(sources) | $(OUTPUT)
	$(CC) $(INCLUDEPATH) -c $< -o $@

$(OUTPUT):
	[ -d $@ ] || mkdir -p $@;

PHONY += clean
clean:
	rm -rf ${all_objects}
	rm -rf ${library}


