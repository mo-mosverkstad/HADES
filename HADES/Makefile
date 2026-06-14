CXX      = g++
RV_CC    = riscv64-unknown-elf-gcc
RV_COPY  = riscv64-unknown-elf-objcopy

CXXFLAGS   = -O2 -Wall -shared -std=c++17 -fPIC
PYBIND_INC = $(shell python3 -m pybind11 --includes)
EXT_SUFFIX = $(shell python3-config --extension-suffix)
RVFLAGS    = -march=rv32i -mabi=ilp32 -nostdlib -T tools/programs/link.ld

BUILDDIR   = build
TARGET     = $(BUILDDIR)/hades$(EXT_SUFFIX)

SRCS = src/hardware/cpu.cpp engine/bridge/bindings.cpp

ASM_SRCS = $(wildcard tools/programs/*.S)
ASM_BINS = $(ASM_SRCS:.S=.bin)

.PHONY: all clean engine asm test

all: engine asm

engine: $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(SRCS) src/hardware/cpu.h src/hardware/memory.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(PYBIND_INC) -Iengine/include $(SRCS) -o $@

asm: $(ASM_BINS)

%.elf: %.S src/programs/link.ld
	$(RV_CC) $(RVFLAGS) -o $@ $<

%.bin: %.elf
	$(RV_COPY) -O binary $< $@

test: engine
	PYTHONPATH=$(BUILDDIR) python3 software/test_cpu.py

run: engine asm
	PYTHONPATH=$(BUILDDIR) python3 software/runner.py $(ARGS)

clean:
	rm -rf $(BUILDDIR) src/programs/*.elf src/programs/*.bin