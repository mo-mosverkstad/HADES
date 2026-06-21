# Compatibility Analysis: New HADES vs programming-learning Projects

**Date:** 2026-06-21  
**Context:** The new HADES project (`github/HADES/HADES/`) is a clean rewrite of the original HADES (`github/programming-learning/HADES/`). This document analyzes whether projects in `programming-learning/` that depend on the old HADES can directly use the new one.

**Conclusion: No, they cannot.** The new HADES covers roughly phases 1–8 of the old project. The external projects depend on features from phases 9–15+ that have not been ported.

---

## 1. Projects That Depend on HADES

### Python consumers (via `import hades` pybind11 module)

| Project | Build Method | Key HADES APIs Used |
|---------|--------------|---------------------|
| **CERBERUS** | `make -C hades engine` → pybind `.so` | `CPU`, `run()`, `uart_send/recv`, `set_interrupts_enabled` |
| **HYPNOS** | `make -C hades engine` → pybind `.so` | `CPU`, `run()`, `radio_send/recv`, `uart_send/recv` |
| **HYPNOS-Net** | Same as HYPNOS | `CPU`, `radio_send/recv`, `uart_send/recv` |
| **IronGuard** | Via embedded `cerberus/hades/` | `CPU`, `set_interrupts_enabled`, `uart_send/recv` |
| **Old HADES demos** | `PYTHONPATH=build` | Full API: power trace, countermeasures, faults, MMU, disk |

### C++ consumers (static library, direct `#include`)

| Project | Include Path | Key Classes Used |
|---------|-------------|------------------|
| **HELIOS** | `-Ihades/layer1_hardware/include` | `CPU`, `MultiCore`, all I/O devices |
| **HYPERION** | `-Ihades/layer1_hardware/include` | `CPU`, `MultiCore` |

---

## 2. Structural Differences

| Aspect | Old HADES (programming-learning) | New HADES (standalone) |
|--------|----------------------------------|------------------------|
| Root layout | `layer1_hardware/`, `layer3_bridge/`, `layer4_software/`, `layer5_attacker/` | `src/hardware/`, `src/bridge/`, `src/software/`, `src/demos/` |
| Header path | `layer1_hardware/include/*.h` | `src/hardware/*.h` |
| Source path | `layer1_hardware/src/*.cpp` | `src/hardware/*.cpp` |
| Bindings | `layer3_bridge/bindings.cpp` | `src/bridge/bindings.cpp` |
| C++ standard | C++17 | C++23 |
| CPU architecture | Single `CPU` class with `set_pipeline_enabled()` toggle | Separate `CPU` (single-cycle) + `PipelinedCPU` (CRTP base) |
| I/O activation | Always active once devices exist | Opt-in via `set_io_enabled(true)` |
| Build output | `build/hades.cpython-*.so` | `build/hades.cpython-*.so` (same module name) |

---

## 3. API Comparison

### Present in BOTH old and new

| Method | Old (`CPU`) | New (`CPU`) | New (`PipelinedCPU`) |
|--------|:-----------:|:-----------:|:--------------------:|
| `load_program(binary, base_addr)` | ✓ | ✓ | ✓ |
| `load_data(data, base_addr)` | ✓ | ✓ | ✓ |
| `run(max_instructions)` | ✓ | ✓ | ✓ |
| `reset()` | ✓ | ✓ | ✓ |
| `get_cycles()` | ✓ | ✓ | ✓ |
| `get_pc()` | ✓ | ✓ | ✓ |
| `get_reg(idx)` | ✓ | ✓ | ✓ |
| `read_mem(addr, len)` | ✓ | ✓ | ✓ |
| `set_cache_enabled(enabled)` | ✓ | ✓ | ✓ |
| `set_miss_penalty(cycles)` | ✓ | ✓ | ✓ |
| `get_icache_misses()` | ✓ | ✓ | ✓ |
| `get_dcache_misses()` | ✓ | ✓ | ✓ |
| `set_mem_hierarchy_enabled(enabled)` | ✓ | ✓ | ✓ |
| `get_sdram_row_hits()` | ✓ | ✓ | ✓ |
| `get_sdram_row_misses()` | ✓ | ✓ | ✓ |
| `uart_send(data)` | ✓ | ✓ | ✓ |
| `uart_recv()` | ✓ | ✓ | ✓ |
| `gpio_set_input(value)` | ✓ | ✓ | ✓ |
| `gpio_get_output()` | ✓ | ✓ | ✓ |
| `vga_get_framebuffer()` | ✓ | ✓ | ✓ |
| `vga_get_char_buffer()` | ✓ | ✓ | ✓ |
| `vga_get_char_row(row)` | ✓ | ✓ | ✓ |

### Present ONLY in new (not in old)

| Method | Notes |
|--------|-------|
| `set_io_enabled(enabled)` | New gating mechanism; old had I/O always active |
| `get_io_enabled()` | |

### Present in old, MISSING from new

#### Category 1: Power Side-Channel Analysis

| Missing Method | Used By |
|----------------|---------|
| `get_power_trace()` | Old HADES demos (CPA attack), IronGuard |
| `set_leakage_model(model)` | Old HADES demos |
| `set_noise(stddev)` | Old HADES demos, IronGuard |
| `set_seed(seed)` | Old HADES demos |
| `LeakageModel` enum (`HAMMING_WEIGHT`, `HAMMING_DISTANCE`) | Old HADES demos |

#### Category 2: Countermeasures

| Missing Method | Used By |
|----------------|---------|
| `set_countermeasure(cm)` | Old HADES demos |
| `set_cache_randomize(enabled, seed)` | Old HADES demos |
| `Countermeasure` enum (`NONE`, `MASKING`, `CONSTANT_POWER`, `SHUFFLED_ORDER`) | Old HADES demos |

#### Category 3: Pipeline Toggle

| Missing Method | Used By |
|----------------|---------|
| `set_pipeline_enabled(enabled)` | Old HADES demos (pipeline comparison) |

*Note: New HADES separates this into two classes (`CPU` = single-cycle, `PipelinedCPU` = pipelined). Old code that toggles mid-run won't work without a shim.*

#### Category 4: Fault Injection

| Missing Method | Used By |
|----------------|---------|
| `set_fault(cycle, reg_idx, mask)` | Old HADES demos (fault injection) |
| `clear_faults()` | Old HADES demos |

#### Category 5: MMU / Virtual Memory

| Missing Method | Used By |
|----------------|---------|
| `set_mmu_satp(satp)` | Old HADES demos, HYPNOS |
| `get_mmu_satp()` | Old HADES demos |
| `mmu_flush_tlb()` | Old HADES demos |
| `get_tlb_hits()` | Old HADES demos |
| `get_tlb_misses()` | Old HADES demos |
| `get_page_faults()` | Old HADES demos, HYPNOS |

#### Category 6: Block Storage Device

| Missing Method | Used By |
|----------------|---------|
| `disk_load_image(data)` | Old HADES demos |
| `disk_save_image()` | Old HADES demos |
| `disk_write_sector(sector, data)` | Old HADES demos |
| `disk_read_sector(sector)` | Old HADES demos |
| `get_disk_reads()` | Old HADES demos |
| `get_disk_writes()` | Old HADES demos |

#### Category 7: Interrupts

| Missing Method | Used By |
|----------------|---------|
| `set_interrupts_enabled(enabled)` | **CERBERUS** (critical), IronGuard |
| `get_interrupts_enabled()` | CERBERUS |

#### Category 8: Radio (802.15.4 / IoT)

| Missing Method | Used By |
|----------------|---------|
| `radio_send(data)` | **HYPNOS**, HYPNOS-Net |
| `radio_recv()` | **HYPNOS**, HYPNOS-Net |

#### Category 9: Other

| Missing Method | Used By |
|----------------|---------|
| `vga_get_color_buffer()` | Old HADES demos (VGA color mode) |
| `set_onchip_size(size)` | Old HADES demos (memory hierarchy) |
| `set_sdram_row_bits(bits)` | Old HADES demos (memory hierarchy) |
| `get_instret()` | Old HADES CPU class (new only has it on PipelinedCPU) |
| `get_perf_counters()` | Old HADES CPU class (new only has it on PipelinedCPU) |

### MultiCore differences

| Old `MultiCore` method | Present in new? |
|------------------------|:---------------:|
| `reset()` | ✓ |
| `load_program(core_id, binary, base_addr)` | ✓ |
| `load_data(data, base_addr)` | ✓ |
| `run(max_cycles)` | ✓ |
| `get_reg(core_id, idx)` | ✓ |
| `get_cycles(core_id)` | ✓ |
| `get_instret(core_id)` | ✓ |
| `get_global_cycles()` | ✓ |
| `is_halted(core_id)` | ✓ |
| `read_mem(addr, len)` | ✓ |
| `get_mutex_contentions()` | ✓ |
| `get_mutex_locked()` | ✓ |
| `get_mutex_owner()` | ✓ |
| `uart_send(data)` | ✓ |
| `uart_recv()` | ✓ |
| `gpio_set_input(value)` | ✓ |
| `gpio_get_output()` | ✓ |
| `get_power_trace(core_id)` | ✗ (missing) |

---

## 4. Bug Fixed in New HADES

During this analysis, a critical bug was found and fixed in `PipelinedCPU::run()`:

**Problem:** The `run()` method compared cumulative cycle/instruction counters against an absolute limit. After the first call exhausted the budget, subsequent calls would immediately exit because the counters had already exceeded the threshold.

**Fix:** Changed to relative comparisons (snapshot counter at start, run until delta reaches budget).

**Affected:** Any code that calls `cpu.run(N)` multiple times (interactive demos, chunk-based execution in CERBERUS's `run.py`).

---

## 5. Options for Adaptation

### Option A: Extend New HADES to Re-Add Missing Features

Port the missing features from the old codebase into the new CRTP-based architecture.

**Pros:**
- Single codebase going forward
- Cleaner architecture (CRTP, C++23, modular)
- All projects converge on one engine

**Cons:**
- Significant development effort (9 feature categories)
- Risk of re-introducing old bugs

**Implementation plan (priority order):**

1. **Interrupts** (unblocks CERBERUS, IronGuard)
   - Add `interrupts_enabled_` flag to `CPUBase`
   - Add timer interrupt check in pipeline cycle
   - Expose via bindings

2. **Power side-channel** (unblocks old demos, IronGuard)
   - Port `leakage.h` → add `Leakage` member to `CPUBase`
   - Add `get_power_trace()`, `set_leakage_model()`, `set_noise()`, `set_seed()`
   - Add `LeakageModel` enum to bindings

3. **Countermeasures** (unblocks old demos)
   - Port `Countermeasure` enum
   - Add masking/shuffling/constant-power logic

4. **Fault injection** (unblocks old demos)
   - Add fault vector and check in execute/writeback stage

5. **Radio** (unblocks HYPNOS, HYPNOS-Net)
   - Add `Radio` IODevice (similar to UART but with packet framing)
   - Register at I/O address `0xF0A0`

6. **MMU** (unblocks old Phase 14 demos)
   - Port `mmu.h` → Sv32 page table walker
   - Add TLB, page fault handling

7. **Block device** (unblocks old Phase 15 demos)
   - Port `block_device.h` → sector-addressable storage
   - Register at I/O address `0xF0C0`

8. **VGA color buffer** (minor)
   - Already partially present in VGA; just expose `get_color_buffer()`

9. **Memory config** (`set_onchip_size`, `set_sdram_row_bits`)
   - Add setters to `CPUBase` forwarding to `MemHierarchy`

10. **Pipeline toggle compatibility**
    - Add `set_pipeline_enabled()` that switches internal implementation, OR
    - Document that users should use `PipelinedCPU` directly

### Option B: Keep Old HADES for External Projects (No Migration)

Leave the `hades/` subdirectory embedded in each external project (CERBERUS, HYPNOS, etc.) untouched. They continue using the old code.

**Pros:**
- Zero effort; everything works today
- No risk of breaking existing projects

**Cons:**
- Two divergent codebases
- Bug fixes (like the `run()` fix) must be manually backported
- Old code uses C++17, less clean architecture
- Maintenance burden grows over time

### Option C: Compatibility Wrapper Layer

Write a Python shim (`compat.py`) that wraps the new HADES module and provides the old API surface by mapping calls where possible.

**Pros:**
- Quick for simple mappings (`set_io_enabled` auto-call, `PipelinedCPU` ↔ `CPU` aliasing)
- Non-invasive to new codebase

**Cons:**
- Cannot implement features that don't exist in C++ (power trace, MMU, disk, radio, faults)
- Only covers Python consumers; C++ consumers (HELIOS, HYPERION) still broken
- Fragile; breaks when API evolves

---

## 6. Recommendation

**Option A is the right path.** The new HADES has a cleaner architecture that makes adding these features easier than in the old monolithic `CPU` class. The CRTP base class pattern means features can be added to `CPUBase<T>` and automatically available to both `CPU` and `PipelinedCPU`.

**Suggested execution order:**

| Phase | Feature | Unblocks | Effort |
|-------|---------|----------|--------|
| 1 | Interrupts | CERBERUS, IronGuard | Low |
| 2 | Power trace + leakage | Old demos, IronGuard | Medium |
| 3 | Radio device | HYPNOS, HYPNOS-Net | Low |
| 4 | Fault injection | Old demos | Low |
| 5 | Countermeasures | Old demos | Medium |
| 6 | MMU (Sv32) | Old demos | High |
| 7 | Block device | Old demos | Medium |
| 8 | Minor (VGA color, mem config, pipeline toggle) | Old demos | Low |

After each phase, the corresponding external project(s) can be migrated by:
1. Replacing their embedded `hades/` with a symlink or path to the new build
2. Updating `PYTHONPATH` from `hades/build` to new HADES `build/`
3. Updating C++ include paths from `hades/layer1_hardware/include` to new `src/hardware/`
4. Changing `-std=c++17` to `-std=c++23` in Makefiles

---

## 7. Migration Checklist Per Project

### CERBERUS
- [ ] Port interrupts to new HADES (Phase 1)
- [ ] Update `Makefile`: change `make -C hades engine` path
- [ ] Update `tools/run.py`: adjust `sys.path` to new build location
- [ ] Verify: `make demo-01` through `demo-05` pass

### HYPNOS / HYPNOS-Net
- [ ] Port radio device to new HADES (Phase 3)
- [ ] Update `Makefile` and `sys.path` in scenarios
- [ ] Verify: `make demo-00` (HADES import) and `make demo-06`+ (radio scenarios)

### IronGuard
- [ ] Requires both interrupts (Phase 1) and power trace (Phase 2)
- [ ] Update embedded cerberus/hades paths
- [ ] Verify: `make demo-00` and aegis engine tests

### HELIOS / HYPERION (C++ consumers)
- [ ] Update include path: `-Ihades/layer1_hardware/include` → `-I<new>/src/hardware`
- [ ] Update Makefile source globs
- [ ] Change `-std=c++17` → `-std=c++23`
- [ ] Verify compilation and linking

### Old HADES demos (programming-learning/HADES/)
- [ ] Requires ALL features ported (Phases 1–8)
- [ ] Update Makefile paths
- [ ] Verify all 15 demos pass
