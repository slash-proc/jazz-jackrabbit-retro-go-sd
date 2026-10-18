# Jazz Jackrabbit (OpenJazz) — headless Linux harness
#
#   make            → build/jj_host
#   make SAN=1      → with AddressSanitizer + UBSan
#
#   ./build/jj_host Jazz.jj -t 20000 -e 1000 -o /tmp/jj

BUILD_DIR = build
TARGET = $(BUILD_DIR)/jj_host

PROCESSOR ?= 0

OJ = ../src/oj
EXT = ../src/ext
PORT = ../src/port

include $(OJ)/sources.mk

OJ_SOURCES = $(addprefix $(OJ)/,$(OJ_FILES)) $(PORT)/jj_core.cpp $(PORT)/jj_sdl.cpp $(PORT)/jj_cxx.cpp $(PORT)/jj_sound.cpp $(PORT)/jj_render.cpp $(PORT)/jj_state.cpp
EXT_SOURCES = $(addprefix $(EXT)/,$(XMP_FILES) $(EXT_FILES))
PORT_SOURCES = \
	$(PORT)/jj_alloc.c \
	$(PORT)/jj_pack.c \
	$(PORT)/jj_file.c \
	$(PORT)/jj_bake.c \
	$(PORT)/jj_music.c \
	jj_host.c

ifeq ($(PROCESSOR),1)
TARGET = ../tools/extractor/build/jazz-assets.wasm
PORT_SOURCES := $(filter-out jj_host.c,$(PORT_SOURCES))
endif

LIVE ?= 0
ifeq ($(LIVE),1)
OPT = -O1 -g -fno-pie -fno-omit-frame-pointer -DJJ_LIVE_TRACE
LDFLAGS += -no-pie -rdynamic
PORT_SOURCES += jj_live.c
endif

SAN ?= 0
OPT ?= -O2 -g
ifeq ($(SAN),1)
OPT += -fsanitize=address,undefined -fno-omit-frame-pointer -DJJ_HOST_SYSTEM_MALLOC
LDFLAGS += -fsanitize=address,undefined
endif

INCLUDES = -I$(PORT) -I$(PORT)/sdl -I$(OJ) -I$(EXT)/xmp -I$(EXT)/miniz -I$(EXT)/stb
WARN = -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function
CFLAGS = $(OPT) -MMD -std=gnu11 -DGNW $(WARN) $(INCLUDES)
GLUE = -include $(PORT)/gnw_jj_glue.h
CXXFLAGS = $(OPT) -MMD -std=gnu++14 -DGNW $(OJ_DEFS) $(WARN) -Wno-class-memaccess \
	-fno-rtti $(INCLUDES) $(GLUE)
EXTFLAGS = $(OPT) -MMD -std=gnu11 -DGNW $(XMP_DEFS) $(EXT_DEFS) -w $(INCLUDES)

OJ_OBJECTS = $(addprefix $(BUILD_DIR)/oj_,$(subst /,_,$(OJ_FILES:.cpp=.o))) \
	$(BUILD_DIR)/p_jj_core.o $(BUILD_DIR)/p_jj_sdl.o $(BUILD_DIR)/p_jj_cxx.o $(BUILD_DIR)/p_jj_sound.o $(BUILD_DIR)/p_jj_render.o $(BUILD_DIR)/p_jj_state.o
EXT_OBJECTS = $(addprefix $(BUILD_DIR)/x_,$(subst /,_,$(XMP_FILES:.c=.o) $(EXT_FILES:.c=.o)))
PORT_OBJECTS = $(addprefix $(BUILD_DIR)/c_,$(notdir $(PORT_SOURCES:.c=.o)))
ifeq ($(PROCESSOR),1)
PORT_OBJECTS += $(BUILD_DIR)/processor.o
endif

$(OJ_OBJECTS) $(EXT_OBJECTS) $(PORT_OBJECTS): Makefile

all: $(TARGET)

# Browser build used by the static local asset converter. Emscripten's
# virtual filesystem receives user-selected files from the page and keeps all
# intermediate/output files inside the worker.
.PHONY: browser
browser:
	$(MAKE) CC=emcc CXX=em++ BUILD_DIR=build/wasm \
	  TARGET=../tools/browser/build/jj_wasm.mjs \
	  CFLAGS="-O2 -MMD -std=gnu11 -DGNW $(OJ_DEFS) $(EXT_DEFS) $(INCLUDES)" \
	  EXTFLAGS="-O2 -MMD -std=gnu11 -DGNW $(XMP_DEFS) $(EXT_DEFS) -w $(INCLUDES)" \
	  CXXFLAGS="-O2 -Wno-macro-redefined -MMD -std=gnu++14 -DGNW $(OJ_DEFS) $(EXT_DEFS) -fno-rtti -fexceptions $(INCLUDES) $(GLUE)" \
	  LDFLAGS="-O2 -fexceptions -sDISABLE_EXCEPTION_CATCHING=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createJJModule -sENVIRONMENT=worker -sNO_EXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=33554432 -sMAXIMUM_MEMORY=536870912 -sSTACK_SIZE=16777216 -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=callMain,FS"

$(BUILD_DIR):
	mkdir -p $@

define oj_rule
$(BUILD_DIR)/oj_$(subst /,_,$(1:.cpp=.o)): $(OJ)/$(1) | $(BUILD_DIR)
	$$(CXX) -c $$(CXXFLAGS) $$< -o $$@
endef
$(foreach f,$(OJ_FILES),$(eval $(call oj_rule,$(f))))

define ext_rule
$(BUILD_DIR)/x_$(subst /,_,$(1:.c=.o)): $(EXT)/$(1) | $(BUILD_DIR)
	$$(CC) -c $$(EXTFLAGS) $$< -o $$@
endef
$(foreach f,$(XMP_FILES) $(EXT_FILES),$(eval $(call ext_rule,$(f))))

$(BUILD_DIR)/p_%.o: $(PORT)/%.cpp | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/c_%.o: $(PORT)/%.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/c_jj_host.o: jj_host.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/c_jj_live.o: jj_live.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/processor.o: ../tools/extractor/processor.cpp ../tools/extractor/catalog.inc | $(BUILD_DIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(TARGET): $(OJ_OBJECTS) $(EXT_OBJECTS) $(PORT_OBJECTS)
	$(CXX) $^ $(LDFLAGS) -lm -o $@

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: processor processor-raw
processor: processor-raw
	node ../tools/extractor/strip_exports.mjs ../tools/extractor/build/jazz-assets.wasm
	/emsdk/upstream/bin/wasm-opt -O2 --remove-unused-module-elements ../tools/extractor/build/jazz-assets.wasm -o ../tools/extractor/build/jazz-assets.opt.wasm
	mv ../tools/extractor/build/jazz-assets.opt.wasm ../tools/extractor/build/jazz-assets.wasm

processor-raw:
	mkdir -p ../tools/extractor/build
	$(MAKE) PROCESSOR=1 CC=emcc CXX=em++ BUILD_DIR=build/wasm-processor \
	  CFLAGS="-O2 -MMD -std=gnu11 -DGNW -DJJ_GWRG_WASM -DJJ_HOST_SYSTEM_MALLOC $(OJ_DEFS) $(EXT_DEFS) $(INCLUDES) -include ../tools/extractor/quiet.h" \
	  EXTFLAGS="-O2 -MMD -std=gnu11 -DGNW -DJJ_GWRG_WASM $(XMP_DEFS) $(EXT_DEFS) -w $(INCLUDES) -include ../tools/extractor/quiet.h" \
	  CXXFLAGS="-O2 -MMD -std=gnu++14 -DGNW -DJJ_GWRG_WASM -DJJ_HOST_SYSTEM_MALLOC $(OJ_DEFS) $(EXT_DEFS) -fno-rtti -fexceptions -fwasm-exceptions $(INCLUDES) $(GLUE) -include ../tools/extractor/quiet.h" \
	  LDFLAGS="-O2 -fexceptions -fwasm-exceptions -sSTANDALONE_WASM=1 -sFILESYSTEM=0 -sENVIRONMENT=worker -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=536870912 -sMAXIMUM_MEMORY=536870912 -sSTACK_SIZE=16777216 -sEXPORT_ALL=0 -sEXPORTED_FUNCTIONS=['_abi_version','_alloc','_input_clear','_input_add','_run','_run_begin','_run_step','_stage_count','_stage_index','_stage_name_ptr','_stage_name_len','_output_count','_output_name_ptr','_output_name_len','_output_ptr','_output_len','_error_ptr','_error_len','_warnings_ptr','_warnings_len'] -Wl,--no-entry -Wl,--export-memory" all

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
