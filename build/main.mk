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
include apps/hello_world/hello_world.mk
include apps/sensor_world/sensor_world.mk
include apps/timer_world/timer_world.mk
include chre_api/chre_api.mk
include core/core.mk
include pal/pal.mk
include platform/platform.mk
include util/util.mk

# Include all build submodules.
include build/hexagon.mk
include build/outdir.mk
include build/clean.mk
