SHELL := /bin/bash

# Path to TI ARM CGT toolchain (adjust to your installation)
TI_ARM_CGT ?= $(HOME)/ti-cgt-arm_20.2.7.LTS

# Path to TI F021 Flash API folder containing F021_API_CortexR4_BE_V3D16_NDS.lib
F021_API_DIR ?= $(HOME)/F021 Flash API/02.01.01

ARMCL := $(TI_ARM_CGT)/bin/armcl

OUT_FILE := tms570_on_board_ecu.out
MAP_FILE := tms570_on_board_ecu.map
XML_LINK_INFO := tms570_on_board_ecu_linkInfo.xml
BUILD_DIR := build

CPU_FLAGS := -mv7R4 --code_state=32 --float_support=VFPv3D16
COMMON_FLAGS := -g --diag_warning=225 --diag_wrap=off --display_error_number --enum_type=packed --abi=eabi
C_FLAGS := --c11
CXX_FLAGS :=

INCLUDES := \
	--include_path="$(CURDIR)" \
	--include_path="$(CURDIR)/include" \
	--include_path="$(CURDIR)/ft81x_driver" \
	--include_path="$(TI_ARM_CGT)/include"

LIB_PATHS := \
	-i"$(TI_ARM_CGT)/lib" \
	-i"$(F021_API_DIR)" \
	-i"$(TI_ARM_CGT)/include"

LIBS := \
	-lrtsv7R4_T_be_v3D16_eabi.lib \
	-lF021_API_CortexR4_BE_V3D16_NDS.lib

LINKER_CMD ?= source/sys_link_ti.cmd

SRC_C := $(filter-out source/sys_main.c,$(wildcard source/*.c)) $(wildcard ft81x_driver/*.c)
SRC_CPP := $(wildcard source/*.cpp) $(wildcard ft81x_driver/*.cpp)
SRC_ASM := $(wildcard source/*.asm) $(wildcard ft81x_driver/*.asm)

PREBUILT_ASM_OBJS := $(addprefix Debug/source/,$(notdir $(SRC_ASM:.asm=.obj)))

OBJS_C := $(patsubst %.c,$(BUILD_DIR)/%.obj,$(SRC_C))
OBJS_CPP := $(patsubst %.cpp,$(BUILD_DIR)/%.obj,$(SRC_CPP))
OBJS := $(OBJS_C) $(OBJS_CPP) $(PREBUILT_ASM_OBJS)

.PHONY: all clean check

all: check $(OUT_FILE)

check:
	@if [[ ! -x "$(ARMCL)" ]]; then \
		echo "ERROR: armcl no encontrado en $(ARMCL)"; \
		echo "Ajusta TI_ARM_CGT o exportalo antes de compilar."; \
		exit 1; \
	fi
	@if [[ ! -f "$(LINKER_CMD)" ]]; then \
		echo "ERROR: linker command file no encontrado: $(LINKER_CMD)"; \
		exit 1; \
	fi
	@for obj in $(PREBUILT_ASM_OBJS); do \
		if [[ ! -f "$$obj" ]]; then \
			echo "ERROR: objeto ASM precompilado no encontrado: $$obj"; \
			exit 1; \
		fi; \
	done

$(OUT_FILE): $(OBJS) $(LINKER_CMD)
	@echo "[LD] $@"
	"$(ARMCL)" $(CPU_FLAGS) $(COMMON_FLAGS) \
		-z -m"$(MAP_FILE)" --heap_size=0x800 --stack_size=0x800 \
		--entry_point=_c_int00 \
		$(LIB_PATHS) --reread_libs --warn_sections \
		--xml_link_info="$(XML_LINK_INFO)" --rom_model --be32 \
		-o "$@" $(OBJS) "$(LINKER_CMD)" $(LIBS)

$(BUILD_DIR)/%.obj: %.c
	@mkdir -p "$(dir $@)"
	@echo "[CC] $<"
	"$(ARMCL)" $(CPU_FLAGS) $(COMMON_FLAGS) $(C_FLAGS) $(INCLUDES) \
		--preproc_with_compile --preproc_dependency="$(@:.obj=.d_raw)" \
		--obj_directory="$(dir $@)" "$<"

$(BUILD_DIR)/%.obj: %.cpp
	@mkdir -p "$(dir $@)"
	@echo "[CXX] $<"
	"$(ARMCL)" $(CPU_FLAGS) $(COMMON_FLAGS) $(CXX_FLAGS) $(INCLUDES) \
		--preproc_with_compile --preproc_dependency="$(@:.obj=.d_raw)" \
		--obj_directory="$(dir $@)" "$<"

$(BUILD_DIR)/source/sys_main.obj: source/sys_main.cpp
	@mkdir -p "$(dir $@)"
	@echo "[CXX] $<"
	"$(ARMCL)" $(CPU_FLAGS) $(COMMON_FLAGS) $(CXX_FLAGS) $(INCLUDES) \
		--preproc_with_compile --preproc_dependency="$(@:.obj=.d_raw)" \
		--obj_directory="$(dir $@)" "$<"

clean:
	rm -rf "$(BUILD_DIR)" "$(OUT_FILE)" "$(MAP_FILE)" "$(XML_LINK_INFO)"