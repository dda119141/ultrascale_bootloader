###############################################################################
# Copyright (c) 2021 Xilinx, Inc.  All rights reserved.
# SPDX-License-Identifier: MIT
###############################################################################
DRIVER_LIB_VERSION = v1.0

library := libqspipsu.a

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
DIR_NAME := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))

include ../common.mk
