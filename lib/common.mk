###############################################################################
# Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
# SPDX-License-Identifier: MIT
###############################################################################
DRIVER_LIB_VERSION = v1.0

library ?= libxil.a

LOCAL_DIR ?= $(CURDIR)

INCLUDEDIR = $(LOCAL_DIR)
INCLUDEDIR += $(LOCAL_DIR)/../include
INCLUDEDIR += $(LOCAL_DIR)/../../

INCLUDEPATH += $(addprefix -I ,$(INCLUDEDIR))

assembler_sources = $(wildcard *.S)
sources = $(wildcard *.c)

objects = $(patsubst %.c, %.o, $(sources))
assembler_objects = $(patsubst %.S, %.o, $(assembler_sources))
all_objects = $(objects)
all_objects += $(assembler_objects)

EXTRA_ARCHIVE_FLAGS=rc

.PHONY: library
library: $(library)

$(library): $(all_objects)
	$(AR) $(EXTRA_ARCHIVE_FLAGS) $@ $^

%.o:%.c
	$(CC) $(INCLUDEPATH) -c $< -o $@

%.o:%.S
	$(CC) $(INCLUDEPATH) -c $< -o $@

PHONY += clean
clean:
	rm -rf ${all_objects}
	rm -rf ${library}


