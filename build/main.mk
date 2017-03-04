#
# This is the main entry-point for the CHRE Makefile build system. Different
# components of the build are included below.
#

ifeq ($(MAKECMDGOALS),)
define EMPTY_GOALS_ERROR
You must specify a target to build. Currently supported targets are:
    - all
    - hexagon
endef
$(error $(EMPTY_GOALS_ERROR))
endif

# The all target to build all source
.PHONY: all
all:

# Include mk files from each subdirectory.
# TODO: Add mk files for core, chre_api, util
include platform/platform.mk

# Include all build submodules.
include build/hexagon.mk
include build/outdir.mk
include build/clean.mk
