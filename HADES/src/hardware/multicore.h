#pragma once
#include <cstdint>
#include <vector>
#include "mem_hierarchy.h"
#include "pipeline.h"
#include "cache.h"
#include "io_bus.h"
#include "timer.h"
#include "uart.h"
#include "gpio.h"
#include "mutex.h"

// Multi-core system: 2 CPUs sharing memory and I/O devices.
// Each CPU has its own:
//   - Register file, PC, pipeline state
//   - L1 I-cache, L1 D-cache
// Shared between cores:
//   - Memory (MemHierarchy)
//   - I/O Bus (Timer, UART, GPIO, Mutex)
//
// Execution model: round-robin (CPU0 step, CPU1 step, repeat)
// This simulates concurrent execution on a shared bus.

class CoreState {
public:
    uint32_t regs[32] = {};
    uint32_t pc = 0x1000;
    bool halted = false;
    uint64_t cycles = 0;
    uint64_t instret = 0;
    Cache icache;
    Cache dcache;

    void reset(uint32_t start_pc) {
        for (int i = 0; i < 32; i++) regs[i] = 0;
        pc = start_pc;
        halted = false;
        cycles = 0;
        instret = 0;
        icache.reset();
        dcache.reset();
    }
};

/**
 * Dual-core CPU controller with shared memory and I/O.
 * Round-robin execution: Core 0 step, Core 1 step, tick devices.
 * Shared: memory, I/O bus (timer, UART, GPIO, mutex).
 * Independent: registers, PC, cycle/instret counters per core.
 * Terminates when both cores halt (ECALL).
 */
class MultiCore {
public:
    MultiCore() { reset(); }

    void reset() {
        cores_[0].reset(0x1000);
        cores_[1].reset(0x2000); // Core 1 starts at different address
        mem_.clear();
        mutex_.reset();
        global_cycle_ = 0;

        // Setup I/O bus with mutex
        io_bus_ = IOBus();
        io_bus_.register_device(0xF000, &timer_);
        io_bus_.register_device(0xF020, &uart_);
        io_bus_.register_device(0xF040, &gpio_);
        io_bus_.register_device(0xF060, &mutex_);
    }

    // Load program for a specific core
    void load_program(int core_id, const std::vector<uint8_t>& binary, uint32_t base_addr) {
        mem_.load(base_addr, binary);
        cores_[core_id].pc = base_addr;
    }

    // Load shared data
    void load_data(const std::vector<uint8_t>& data, uint32_t base_addr) {
        mem_.load(base_addr, data);
    }

    // Run both cores for max_cycles (round-robin)
    void run(uint32_t max_cycles = 100000) {
        for (uint32_t c = 0; c < max_cycles; c++) {
            global_cycle_++;

            // Step core 0
            if (!cores_[0].halted) {
                step_core(0);
            }

            // Step core 1
            if (!cores_[1].halted) {
                step_core(1);
            }

            // Tick I/O devices
            io_bus_.tick_all();

            // Both halted → done
            if (cores_[0].halted && cores_[1].halted) break;
        }
    }

    // Observation
    uint32_t get_reg(int core_id, uint32_t idx) const {
        return (idx < 32) ? cores_[core_id].regs[idx] : 0;
    }

    uint64_t get_cycles(int core_id) const { return cores_[core_id].cycles; }
    uint64_t get_instret(int core_id) const { return cores_[core_id].instret; }
    uint64_t get_global_cycles() const { return global_cycle_; }
    bool is_halted(int core_id) const { return cores_[core_id].halted; }

    std::vector<uint8_t> read_mem(uint32_t addr, uint32_t len) const {
        return mem_.dump(addr, len);
    }

    // Mutex stats
    uint64_t get_mutex_contentions() const { return mutex_.get_contention_count(); }
    bool get_mutex_locked() const { return mutex_.is_locked(); }
    uint32_t get_mutex_owner() const { return mutex_.get_owner(); }

    // UART/GPIO (shared)
    void uart_send(const std::vector<uint8_t>& data) { uart_.host_send(data); }
    std::vector<uint8_t> uart_recv() { return uart_.host_recv(); }
    void gpio_set_input(uint32_t value) { gpio_.set_input(value); }
    uint32_t gpio_get_output() const { return gpio_.get_output(); }

private:
    CoreState cores_[2];
    MemHierarchy mem_;
    IOBus io_bus_;
    Timer timer_;
    UART uart_;
    GPIO gpio_;
    Mutex mutex_;
    uint64_t global_cycle_;

    // Single-cycle step for one core
    void step_core(int id) {
        CoreState& c = cores_[id];
        c.cycles++;

        uint32_t instr = mem_.read_word(c.pc);

        // Decode
        uint32_t opcode = instr & 0x7F;
        uint32_t rd = (instr >> 7) & 0x1F;
        uint32_t funct3 = (instr >> 12) & 0x7;
        uint32_t rs1 = (instr >> 15) & 0x1F;
        uint32_t rs2 = (instr >> 20) & 0x1F;
        uint32_t funct7 = (instr >> 25) & 0x7F;

        int32_t imm_i = (int32_t)instr >> 20;
        int32_t imm_s = ((instr >> 25) << 5) | ((instr >> 7) & 0x1F);
        if (imm_s & 0x800) imm_s |= 0xFFFFF000;
        int32_t imm_b = 0;
        imm_b |= ((instr >> 31) & 1) << 12;
        imm_b |= ((instr >> 7) & 1) << 11;
        imm_b |= ((instr >> 25) & 0x3F) << 5;
        imm_b |= ((instr >> 8) & 0xF) << 1;
        if (imm_b & 0x1000) imm_b |= 0xFFFFE000;
        int32_t imm_u = (int32_t)(instr & 0xFFFFF000);
        int32_t imm_j = 0;
        imm_j |= ((instr >> 31) & 1) << 20;
        imm_j |= ((instr >> 12) & 0xFF) << 12;
        imm_j |= ((instr >> 20) & 1) << 11;
        imm_j |= ((instr >> 21) & 0x3FF) << 1;
        if (imm_j & 0x100000) imm_j |= 0xFFE00000;

        auto write_reg = [&](uint32_t r, uint32_t val) {
            if (r == 0) return;
            c.regs[r] = val;
        };

        auto mem_read = [&](uint32_t addr) -> uint32_t {
            if (io_bus_.is_io_address(addr)) return io_bus_.read(addr);
            return mem_.read_word(addr);
        };

        auto mem_read_byte = [&](uint32_t addr) -> uint8_t {
            if (io_bus_.is_io_address(addr)) return (uint8_t)io_bus_.read(addr);
            return mem_.read_byte(addr);
        };

        auto mem_write = [&](uint32_t addr, uint32_t val) {
            if (io_bus_.is_io_address(addr)) { io_bus_.write(addr, val); return; }
            mem_.write_word(addr, val);
        };

        auto mem_write_byte = [&](uint32_t addr, uint8_t val) {
            if (io_bus_.is_io_address(addr)) { io_bus_.write(addr, val); return; }
            mem_.write_byte(addr, val);
        };

        switch (opcode) {
        case 0b0110111: // LUI
            write_reg(rd, (uint32_t)imm_u);
            c.pc += 4; break;
        case 0b0010111: // AUIPC
            write_reg(rd, c.pc + (uint32_t)imm_u);
            c.pc += 4; break;
        case 0b1101111: { // JAL
            uint32_t ret = c.pc + 4;
            c.pc = c.pc + (uint32_t)imm_j;
            write_reg(rd, ret); break;
        }
        case 0b1100111: { // JALR
            uint32_t ret = c.pc + 4;
            c.pc = (c.regs[rs1] + (uint32_t)imm_i) & ~1u;
            write_reg(rd, ret); break;
        }
        case 0b1100011: { // BRANCH
            int32_t a = (int32_t)c.regs[rs1], b = (int32_t)c.regs[rs2];
            uint32_t ua = c.regs[rs1], ub = c.regs[rs2];
            bool taken = false;
            switch (funct3) {
                case 0: taken = (a == b); break;
                case 1: taken = (a != b); break;
                case 4: taken = (a < b); break;
                case 5: taken = (a >= b); break;
                case 6: taken = (ua < ub); break;
                case 7: taken = (ua >= ub); break;
            }
            c.pc = taken ? c.pc + (uint32_t)imm_b : c.pc + 4;
            break;
        }
        case 0b0000011: { // LOAD
            uint32_t addr = c.regs[rs1] + (uint32_t)imm_i;
            uint32_t val = 0;
            switch (funct3) {
                case 0: { int8_t b = (int8_t)mem_read_byte(addr); val = (uint32_t)(int32_t)b; break; }
                case 2: val = mem_read(addr); break;
                case 4: val = mem_read_byte(addr); break;
            }
            write_reg(rd, val);
            c.pc += 4; break;
        }
        case 0b0100011: { // STORE
            uint32_t addr = c.regs[rs1] + (uint32_t)imm_s;
            switch (funct3) {
                case 0: mem_write_byte(addr, c.regs[rs2] & 0xFF); break;
                case 2: mem_write(addr, c.regs[rs2]); break;
            }
            c.pc += 4; break;
        }
        case 0b0010011: { // OP-IMM
            uint32_t src = c.regs[rs1];
            uint32_t result = 0;
            switch (funct3) {
                case 0: result = src + (uint32_t)imm_i; break;
                case 4: result = src ^ (uint32_t)imm_i; break;
                case 6: result = src | (uint32_t)imm_i; break;
                case 7: result = src & (uint32_t)imm_i; break;
                case 1: result = src << (imm_i & 0x1F); break;
                case 5: result = (funct7 & 0x20) ? (uint32_t)((int32_t)src >> (imm_i & 0x1F)) : src >> (imm_i & 0x1F); break;
                case 2: result = ((int32_t)src < imm_i) ? 1 : 0; break;
                case 3: result = (src < (uint32_t)imm_i) ? 1 : 0; break;
            }
            write_reg(rd, result);
            c.pc += 4; break;
        }
        case 0b0110011: { // OP-REG
            uint32_t a = c.regs[rs1], b = c.regs[rs2];
            uint32_t result = 0;
            switch (funct3) {
                case 0: result = (funct7 & 0x20) ? (a - b) : (a + b); break;
                case 1: result = a << (b & 0x1F); break;
                case 2: result = ((int32_t)a < (int32_t)b) ? 1 : 0; break;
                case 3: result = (a < b) ? 1 : 0; break;
                case 4: result = a ^ b; break;
                case 5: result = (funct7 & 0x20) ? (uint32_t)((int32_t)a >> (b & 0x1F)) : a >> (b & 0x1F); break;
                case 6: result = a | b; break;
                case 7: result = a & b; break;
            }
            write_reg(rd, result);
            c.pc += 4; break;
        }
        case 0b1110011: // ECALL
            c.halted = true; break;
        default:
            c.halted = true; break;
        }

        c.regs[0] = 0;
        c.instret++;
    }
};