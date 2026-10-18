# Jazz Jackrabbit (OpenJazz) for Game & Watch Retro-Go SD — GWHB homebrew.
#
#   make                  — build + pack → Jazz.bin
#   make docker           — same build inside the Retro-Go SD builder image
#   make harness          — headless Linux test harness (harness/jj_host.c)
#
# SD card (Retro-Go SD v2.0.0 or later):
#   /homebrews/Jazz.bin   (this homebrew)
#   /homebrews/Jazz.jj    (game data: tools/jj_pack.py, same name)
#
# Verbose compiler lines: make V=

#######################################
# Project identity
#######################################
PROJECT_KIND := homebrew

CORE_NAME  := jazz-jackrabbit
CORE_ENTRY := app_main

HB_NAME    := Jazz Jackrabbit
PACKED_BIN := Jazz.bin

OJ_SRC  := src/oj
EXT_SRC := src/ext
JJ_PORT := src/port

include $(OJ_SRC)/sources.mk

# OpenJazz (JJ1) on the mini SDL of the port
OJ_CXX_SOURCES := $(addprefix $(OJ_SRC)/,$(OJ_FILES)) \
	$(JJ_PORT)/jj_core.cpp $(JJ_PORT)/jj_sdl.cpp $(JJ_PORT)/jj_sound.cpp $(JJ_PORT)/jj_state.cpp

CORE_CXX_SOURCES := $(OJ_CXX_SOURCES) $(JJ_PORT)/jj_cxx.cpp

# Game & Watch side: heap, asset pack, files, music, retro-go glue
CORE_C_SOURCES := \
$(EXT_SRC)/stb/stb_rect_pack.c \
$(EXT_SRC)/miniz/miniz.c \
$(JJ_PORT)/jj_alloc.c \
$(JJ_PORT)/jj_pack.c \
$(JJ_PORT)/jj_file.c \
$(JJ_PORT)/jj_bake.c \
$(JJ_PORT)/jj_libc.c \
$(JJ_PORT)/jj_music.c \
$(JJ_PORT)/jj_main.c

CORE_C_INCLUDES := -I$(JJ_PORT) -I$(JJ_PORT)/sdl -I$(OJ_SRC) -I$(EXT_SRC)/stb -I$(EXT_SRC)/miniz

CORE_C_DEFS := -DPROJECT_KIND_HOMEBREW=1 -DJJ_DEVICE=1

OPT := -O2

# C++ exceptions (OpenJazz uses them): the unwind tables are kept
# (src/port/jj_core.ld) and libsupc++ / libgcc provide the runtime.
CORE_LDSCRIPT := $(JJ_PORT)/jj_core.ld
CORE_LDLIBS := -Wl,--start-group -lsupc++ -lgcc -Wl,--end-group \
	-Wl,--defsym=malloc=jj_malloc -Wl,--defsym=free=jj_free \
	-Wl,--defsym=abort=core_abort -Wl,--defsym=memcmp=core_memcmp \
	-Wl,--defsym=strcmp=core_strcmp -Wl,--defsym=strchr=core_strchr \
	-Wl,--defsym=getenv=jj_getenv -Wl,--defsym=strtoul=jj_strtoul

GNW_CORE_SDK ?= sdk
BUILD_DIR ?= build/$(PROJECT_KIND)

include $(GNW_CORE_SDK)/Makefile

# The SDK's C++ runtime has a bump allocator without free for operator new:
# jj_cxx.cpp replaces it (built, not linked).
CXX_OBJECTS := $(filter-out $(BUILD_DIR)/gw_core_cxx_support.o,$(CXX_OBJECTS))

#######################################
# OpenJazz specific flags
#######################################
OJ_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(OJ_CXX_SOURCES:.cpp=.o)))
OJ_FLAGS := -DGNW $(OJ_DEFS) $(EXT_DEFS) -include $(JJ_PORT)/gnw_jj_glue.h -std=gnu++14 -fexceptions \
	-Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
	-Wno-class-memaccess -Wno-unused-parameter
$(OJ_OBJECTS) $(BUILD_DIR)/jj_cxx.o: CXXFLAGS += $(OJ_FLAGS)
$(BUILD_DIR)/stb_rect_pack.o $(BUILD_DIR)/miniz.o: CFLAGS += -include $(JJ_PORT)/gnw_jj_glue.h -w \
	$(EXT_DEFS) -DMINIZ_NO_STDIO -DMINIZ_NO_TIME
$(OJ_OBJECTS) $(C_OBJECTS): Makefile

#######################################
# Packed header version
#######################################
CORE_VERSION ?= $(shell git describe --tags --dirty 2>/dev/null || echo NOTAG)

#######################################
# Pack (GWHB)
#######################################
PACK_HOMEBREW := $(GNW_CORE_SDK)/tools/pack_homebrew.py
GEN_COVER     := scripts/gen_homebrew_cover.py
COVER_JPG     := $(BUILD_DIR)/cover.jpg
COVER_WIDTH   ?= 128
COVER_HEIGHT  ?= 96

.PHONY: pack cover
cover: $(COVER_JPG)

# Must stay <= 186x100 and <= 10 KiB (firmware cover cache).
$(COVER_JPG): $(GEN_COVER) | $(BUILD_DIR)
	$(V)$(ECHO) "[ COVER ] $(COVER_JPG) $(COVER_WIDTH)x$(COVER_HEIGHT)"
	$(V)python3 $(GEN_COVER) \
		--out $(COVER_JPG) \
		--title "$(HB_NAME)" \
		--width $(COVER_WIDTH) \
		--height $(COVER_HEIGHT)

pack: $(TARGET_BIN) $(COVER_JPG)
	$(V)$(ECHO) [ PACK GWHB ] $(PACKED_BIN) version=$(CORE_VERSION)
	$(V)python3 $(PACK_HOMEBREW) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--name "$(HB_NAME)" --version "$(CORE_VERSION)" \
		--cover $(COVER_JPG) \
		--out $(PACKED_BIN)

all: pack

clean::
	$(V)rm -f $(PACKED_BIN) $(COVER_JPG)

# Read-only helpers for CI / scripts (make print-PACKED_BIN, etc.).
.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-DOCKER_IMAGE \
	print-TARGET_ELF print-TARGET_MAP print-CORE_VERSION
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-DOCKER_IMAGE:
	@echo $(DOCKER_IMAGE)
print-TARGET_ELF:
	@echo $(TARGET_ELF)
print-TARGET_MAP:
	@echo $(BUILD_DIR)/$(CORE_NAME)_core.map
print-CORE_VERSION:
	@echo $(CORE_VERSION)

#######################################
# Headless Linux harness (same game + port code, virtual clock)
#######################################
.PHONY: harness
harness:
	$(MAKE) -C harness

#######################################
# Docker (same image as the firmware repo)
#######################################
.PHONY: docker docker_pull docker_shell

RELEASE_VERSION ?= v1.5
DOCKER_REPOSITORY ?= sylverb/retro-go-sd-builder
DOCKER_IMAGE ?= $(DOCKER_REPOSITORY):$(RELEASE_VERSION)

DOCKER_TTY_FLAG := $(shell if [ -t 0 ]; then echo -it; else echo; fi)
DOCKER_USER := $(shell id -u):$(shell id -g)
DOCKER_RUN := docker run --rm $(DOCKER_TTY_FLAG) \
	--user $(DOCKER_USER) \
	-v "$(CURDIR):/opt/workdir" \
	-w /opt/workdir \
	$(DOCKER_IMAGE)

docker:
	$(V)$(ECHO) "[ DOCKER ]" $(DOCKER_IMAGE)
	$(V)$(DOCKER_RUN) make --no-print-directory -j$$(nproc)

docker_pull:
	$(V)docker pull $(DOCKER_IMAGE)

docker_shell:
	$(DOCKER_RUN) bash
