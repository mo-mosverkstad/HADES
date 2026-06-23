#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "memory.h"
#include "io_bus.h"
#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "vga.h"
#include "pipeline.h"
#include "executor.h"

struct Decoded; // forward declare

struct ExecResult {
    uint32_t result;
    bool writes_rd;
    bool is_load;
    bool is_store;
    bool is_branch;
    bool branch_taken;
    uint32_t branch_target;
    uint32_t store_value;
};

// Pipelined CPU model.
class PipelinedCPU {
public:
    PipelinedCPU();

    // Execution
    void run(uint32_t max_instructions = 1000000);
    void stop() { exec_.stop(); }
    bool is_running() const { return exec_.is_running(); }
    void reset();

    // State access
    uint32_t get_pc() const { return pc_; }
    uint32_t get_reg(uint32_t idx) const { return (idx < 32) ? regs_[idx] : 0; }
    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const { return mem_.dump(addr, len); }
    uint64_t get_cycles() const { return perf_.mcycle; }
    uint64_t get_instret() const { return perf_.minstret; }
    PerfCounters get_perf_counters() const { return perf_; }
    bool is_halted() const { return halted_ && !memwb_.valid; }

    void load_program(const std::vector<uint8_t>& binary, uint32_t base_addr = 0x1000) {
        mem_.load(base_addr, binary);
        pc_ = base_addr;
        halted_ = false;
    }
    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr = 0x0000) {
        mem_.load(base_addr, data);
    }

    // Configuration
    void set_cache_enabled(bool enabled) { mem_.set_cache_enabled(enabled); }
    void set_miss_penalty(uint32_t cycles) { mem_.set_miss_penalty(cycles); }
    void set_mem_hierarchy_enabled(bool enabled) { mem_.set_hierarchy_enabled(enabled); }

    // Stats
    uint64_t get_icache_misses() const { return mem_.imem().get_cache_misses(); }
    uint64_t get_dcache_misses() const { return mem_.dmem().get_cache_misses(); }
    uint64_t get_sdram_row_hits() const { return mem_.sdram().get_row_hits(); }
    uint64_t get_sdram_row_misses() const { return mem_.sdram().get_row_misses(); }

    // I/O (thread-safe devices)
    void set_io_enabled(bool enabled) { io_enabled_ = enabled; }
    bool get_io_enabled() const { return io_enabled_; }
    void uart_send(const std::vector<uint8_t>& data) { uart_.host_send(data); }
    std::vector<uint8_t> uart_recv() { return uart_.host_recv(); }
    void gpio_set_input(uint32_t value) { gpio_.set_input(value); }
    uint32_t gpio_get_output() const { return gpio_.get_output(); }
    std::vector<uint16_t> vga_get_framebuffer() const { return vga_.get_framebuffer(); }
    std::vector<uint8_t> vga_get_char_buffer() const { return vga_.get_char_buffer(); }
    std::string vga_get_char_row(uint32_t row) const { return vga_.get_char_row(row); }

private:
    uint32_t regs_[32]{};
    uint32_t pc_ = 0x1000;
    bool halted_ = false;

    bool io_enabled_ = false;
    IOBus io_bus_;
    Memory mem_{io_bus_, io_enabled_};
    Timer timer_;
    UART uart_;
    GPIO gpio_;
    VGA vga_;

    // Pipeline stages
    StageIFID ifid_{};
    StageEX ex_{};
    StageMEMWB memwb_{};
    PerfCounters perf_;
    std::unordered_map<uint32_t, uint32_t> csrs_;

    Executor exec_{[this]{ step(); }, [this]{ return is_halted(); }};

    void write_reg(uint32_t rd, uint32_t value) {
        if (rd != 0) regs_[rd] = value;
    }

    void step();  // one pipeline cycle
    void pipeline_cycle();
    void stage_writeback();
    void stage_memory();
    void stage_execute();
    void stage_fetch_decode();
    uint32_t forward_reg(uint32_t reg_idx) const;
    bool detect_load_use_hazard() const;
    static ExecResult execute_alu(const Decoded& d, uint32_t rs1_val, uint32_t rs2_val, uint32_t pc);
    uint32_t csr_read(uint32_t addr) const;
    void csr_write(uint32_t addr, uint32_t value);
};
