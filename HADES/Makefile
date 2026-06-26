CXX      = g++
RV_CC    = riscv64-unknown-elf-gcc
RV_COPY  = riscv64-unknown-elf-objcopy

CXXFLAGS   = -O2 -Wall -shared -std=c++23 -fPIC
PYBIND_INC = $(shell python3 -m pybind11 --includes)
EXT_SUFFIX = $(shell python3-config --extension-suffix)
RVFLAGS    = -march=rv32i -mabi=ilp32 -nostdlib -T src/programs/link.ld

BUILDDIR   = build
TARGET     = $(BUILDDIR)/hades$(EXT_SUFFIX)

SRCS = src/hardware/cpu.cpp src/hardware/pipelined_cpu.cpp src/bridge/bindings.cpp

ASM_SRCS = $(wildcard src/programs/*.S)
ASM_BINS = $(ASM_SRCS:.S=.bin)

.PHONY: all clean engine asm test run run-asm

all: engine asm

engine: $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(SRCS) src/hardware/cpu.h src/hardware/memory.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(PYBIND_INC) -Isrc/hardware $(SRCS) -o $@

asm: $(ASM_BINS)

%.elf: %.S src/programs/link.ld
	$(RV_CC) $(RVFLAGS) -o $@ $<

%.bin: %.elf
	$(RV_COPY) -O binary $< $@

test: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/software/test_cpu.py

run: engine asm
	PYTHONPATH=$(BUILDDIR):src python3 src/software/runner.py $(ARGS)

run-asm: engine
	$(RV_CC) $(RVFLAGS) -o $(FILE:.S=.elf) $(FILE)
	$(RV_COPY) -O binary $(FILE:.S=.elf) $(FILE:.S=.bin)
	PYTHONPATH=$(BUILDDIR):src python3 src/software/runner.py $(FILE:.S=.bin) --dump-regs

demo-01: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_01_basic_cpu.py

demo-02: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_02_pipeline.py

demo-03: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_03_cache.py

demo-04: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_04_memory_hierarchy.py

demo-05: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_05_io_devices.py

demo-06: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_06_multicore.py

demo-07: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_07_vga.py

demo-08: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_08_game.py

demo-08b: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_08b_game_asm.py

demo-08c: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_08c_game_interactive.py

demo-08d: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_08d_game_interactive.py

demo-11: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_11_echo_terminal.py

demo-12: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_12_mmu.py

demo-13: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_13_disk.py

demo-14: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_14_interrupts.py

demo-all: engine
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_01_basic_cpu.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_02_pipeline.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_03_cache.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_04_memory_hierarchy.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_05_io_devices.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_06_multicore.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_07_vga.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_08_game.py
	@echo ""
	PYTHONPATH=$(BUILDDIR):src python3 src/demos/demo_11_echo_terminal.py

clean:
	rm -rf $(BUILDDIR) src/programs/*.elf src/programs/*.bin