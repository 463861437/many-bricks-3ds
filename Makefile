# Build with the official devkitARM/libctru rules.
.SUFFIXES:
.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is unset; run ./build.sh or load your devkitPro environment)
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export DEVKITPRO DEVKITARM

APP_TITLE := Many Bricks 3DS
APP_DESCRIPTION := Touch arcade with hundreds of balls
APP_AUTHOR := Xiaoguai
export APP_TITLE APP_DESCRIPTION APP_AUTHOR

TARGET := many-bricks-3ds
BUILD := build
TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -std=gnu11 -g -O2 -Wall -Wextra -Werror -mword-relocations \
          -ffunction-sections -fdata-sections $(ARCH) $(INCLUDE) -D__3DS__
CXXFLAGS := $(filter-out -std=gnu11,$(CFLAGS)) -std=gnu++17
LDFLAGS = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lctru -lm

ifneq ($(BUILD),$(notdir $(CURDIR)))
export TOPDIR := $(CURDIR)
export APP_ICON := $(CURDIR)/packaging/icon.png
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(CURDIR)/source
export DEPSDIR := $(CURDIR)/$(BUILD)
export INCLUDE := -I$(CURDIR)/include -I$(CTRULIB)/include
export LIBPATHS := -L$(CTRULIB)/lib
export OFILES := $(notdir $(patsubst %.cpp,%.o,$(wildcard source/*.cpp)))
export LD := $(CXX)
export _3DSXFLAGS := --smdh=$(OUTPUT).smdh

.PHONY: all clean cia
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(TOPDIR)/Makefile

cia:
	@./build-cia.sh

$(BUILD):
	@mkdir -p $@

clean:
	rm -rf $(BUILD) build-cia $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia $(TARGET).map

else
.PHONY: all
all: $(OUTPUT).3dsx
$(OUTPUT).3dsx: $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
