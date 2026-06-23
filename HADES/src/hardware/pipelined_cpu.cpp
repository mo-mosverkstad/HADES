#include "pipelined_cpu.h"
#include "rv32_decode.h"
#include <iostream>

PipelinedCPU::PipelinedCPU() { reset(); }

void PipelinedCPU::reset() {
    for (int i = 0; i < 32; i++) regs_[i] = 0;
    pc_ = 0x1000;
    halted_ = false;
    io_enabled_ = false;
    ifid_ = {};
    ex_ = {};
    memwb_ = {};
    perf_.reset();
    csrs_.clear();
    mem_.reset();
    timer_.reset();
    uart_.reset();
    gpio_.reset();
    vga_.reset();
    // Register I/O devices on bus
    io_bus_ = IOBus();
    io_bus_.register_device(0xF000, &timer_);
    io_bus_.register_device(0xF020, &uart_);
    io_bus_.register_device(0xF040, &gpio_);
    io_bus_.register_device(0xF080, &vga_);
}

/*
// cpu.run(N) means "continue executing from where the CPU is currently paused, for up to N instructions."
void PipelinedCPU::run(uint32_t max_instructions) {
    uint64_t start_cycles = perf_.mcycle;
    uint64_t start_instret = perf_.minstret;
    uint64_t max_cycles = (uint64_t)max_instructions * 4;
    while (perf_.mcycle - start_cycles < max_cycles) {
        pipeline_cycle();
        if (halted_ && !memwb_.valid) break;
        if (perf_.minstret - start_instret >= max_instructions) break;
    }
}
*/

void PipelinedCPU::run(uint32_t max_instructions) {
    if (running_) return;  // already executing — ignore re-entrant call
    if (!thread_started_) {
        exec_thread_ = std::thread(&PipelinedCPU::thread_main, this);
        thread_started_ = true;
    }
    stop_requested_ = false;
    if (max_instructions == 0) {
        // Free-running mode: signal thread to run indefinitely, return immediately
        signal_run(0);
        return;
    }
    // Bounded mode: signal thread to run N instructions, block until done
    signal_run(max_instructions);
    wait_for_completion();
}

void PipelinedCPU::thread_main() {
    while (true) {
        wait_for_run_signal();  // sleep until run() is called
        if (shutdown_) return;
        running_ = true;
        run_pipeline(budget_, [&](){ return stop_requested_.load(std::memory_order_relaxed); });
        running_ = false;

        notify_completion();  // wake up blocking run(N) caller if any
    }
}

template<typename Predicate>
void PipelinedCPU::run_pipeline(uint64_t max_instructions, Predicate check_stop) {
    uint64_t start_cycles = perf_.mcycle;
    uint64_t start_instret = perf_.minstret;
    uint64_t max_cycles = max_instructions * 4;
    while (max_instructions == 0 || perf_.mcycle - start_cycles < max_cycles) {
        if (check_stop()) break;
        pipeline_cycle();
        if (halted_ && !memwb_.valid) break;
        if (max_instructions != 0 && perf_.minstret - start_instret >= max_instructions) break;
    }
}

// ─── Pipeline Cycle ─────────────────────────────────────────────────────

void PipelinedCPU::pipeline_cycle() {
    perf_.mcycle++;
    // Tick I/O devices
    if (io_enabled_) io_bus_.tick_all();
    if (detect_load_use_hazard()) {
        perf_.stalls_data++;
        stage_writeback();
        stage_memory();
        ex_ = {};
        return;
    }

    stage_writeback();
    stage_memory();

    if (halted_) {
        ex_ = {};
        ifid_ = {};
        return;
    }

    stage_execute();
    stage_fetch_decode();
}

// ─── Stage: Writeback ───────────────────────────────────────────────────

void PipelinedCPU::stage_writeback() {
    if (!memwb_.valid) return;
    if (memwb_.writes_rd && memwb_.rd != 0) {
        write_reg(memwb_.rd, memwb_.result);
    }
    perf_.minstret++;
}

// ─── Stage: Memory ──────────────────────────────────────────────────────

void PipelinedCPU::stage_memory() {
    memwb_ = {};
    if (!ex_.valid) return;

    memwb_.valid = true;
    memwb_.rd = ex_.rd;
    memwb_.writes_rd = ex_.writes_rd;
    memwb_.is_load = ex_.is_load;

    if (ex_.is_load) {
        uint32_t addr = ex_.alu_result;
        uint32_t f3 = (ex_.instr >> 12) & 0x7;

        uint32_t val = 0;
        switch (f3) {
            case 0b000: { int8_t b = (int8_t)mem_.dmem().read_byte(addr); val = (uint32_t)(int32_t)b; break; }
            case 0b001: { int16_t h = (int16_t)mem_.dmem().read_half(addr); val = (uint32_t)(int32_t)h; break; }
            case 0b010: val = mem_.dmem().read_word(addr); break;
            case 0b100: val = mem_.dmem().read_byte(addr); break;
            case 0b101: val = mem_.dmem().read_half(addr); break;
        }
        perf_.mcycle += mem_.dmem().drain_penalty();
        memwb_.result = val;
    } else if (ex_.is_store) {
        uint32_t addr = ex_.alu_result;
        uint32_t f3 = (ex_.instr >> 12) & 0x7;

        switch (f3) {
            case 0b000: mem_.dmem().write_byte(addr, ex_.rs2_val & 0xFF); break;
            case 0b001: mem_.dmem().write_half(addr, ex_.rs2_val & 0xFFFF); break;
            case 0b010: mem_.dmem().write_word(addr, ex_.rs2_val); break;
        }
        mem_.dmem().drain_penalty();

        memwb_.writes_rd = false;
    } else {
        memwb_.result = ex_.alu_result;
    }
}

// ─── Stage: Execute ─────────────────────────────────────────────────────

void PipelinedCPU::stage_execute() {
    ex_ = {};
    if (!ifid_.valid) return;

    Decoded d = decode_instr(ifid_.instr);

    if (d.opcode == OP_SYSTEM) {
        if (d.funct3 != 0) {
            uint32_t csr_addr = (ifid_.instr >> 20) & 0xFFF;
            uint32_t rs1_val = forward_reg(d.rs1);
            uint32_t old_val = csr_read(csr_addr);
            switch (d.funct3) {
                case 0b001: csr_write(csr_addr, rs1_val); break;
                case 0b010: csr_write(csr_addr, old_val | rs1_val); break;
                case 0b011: csr_write(csr_addr, old_val & ~rs1_val); break;
            }
            ex_.valid = true;
            ex_.instr = ifid_.instr;
            ex_.pc = ifid_.pc;
            ex_.rd = d.rd;
            ex_.writes_rd = (d.rd != 0);
            ex_.alu_result = old_val;
            return;
        }
        ex_.valid = true;
        ex_.instr = ifid_.instr;
        ex_.pc = ifid_.pc;
        ex_.writes_rd = false;
        ex_.is_load = false;
        ex_.is_store = false;
        ex_.is_branch = false;
        halted_ = true;
        return;
    }

    uint32_t rs1_val = forward_reg(d.rs1);
    uint32_t rs2_val = forward_reg(d.rs2);

    ExecResult er = execute_alu(d, rs1_val, rs2_val, ifid_.pc);

    ex_.valid = true;
    ex_.instr = ifid_.instr;
    ex_.pc = ifid_.pc;
    ex_.alu_result = er.result;
    ex_.rs2_val = er.store_value;
    ex_.rd = d.rd;
    ex_.writes_rd = er.writes_rd;
    ex_.is_load = er.is_load;
    ex_.is_store = er.is_store;
    ex_.is_branch = er.is_branch;
    ex_.branch_taken = er.branch_taken;
    ex_.branch_target = er.branch_target;

    if (er.is_branch && er.branch_taken) {
        pc_ = er.branch_target;
        ifid_ = {};
        perf_.stalls_branch++;
    }
}

// ─── Stage: Fetch/Decode ────────────────────────────────────────────────

void PipelinedCPU::stage_fetch_decode() {
    if (ex_.is_branch && ex_.branch_taken) return;

    ifid_.instr = mem_.imem().read_word(pc_);
    perf_.mcycle += mem_.imem().drain_penalty();

    ifid_.pc = pc_;
    ifid_.valid = true;
    pc_ += 4;
}

// ─── Forwarding ─────────────────────────────────────────────────────────

uint32_t PipelinedCPU::forward_reg(uint32_t reg_idx) const {
    if (reg_idx == 0) return 0;
    if (ex_.valid && ex_.writes_rd && ex_.rd == reg_idx && !ex_.is_load) {
        return ex_.alu_result;
    }
    if (memwb_.valid && memwb_.writes_rd && memwb_.rd == reg_idx) {
        return memwb_.result;
    }
    return regs_[reg_idx];
}

// ─── Hazard Detection ───────────────────────────────────────────────────

bool PipelinedCPU::detect_load_use_hazard() const {
    if (!ex_.valid || !ex_.is_load || !ifid_.valid) return false;

    Decoded d = decode_instr(ifid_.instr);
    uint32_t load_rd = ex_.rd;

    bool uses_rs1 = (d.opcode != OP_LUI && d.opcode != OP_AUIPC && d.opcode != OP_JAL);
    bool uses_rs2 = (d.opcode == OP_REG || d.opcode == OP_BRANCH || d.opcode == OP_STORE);

    if (uses_rs1 && d.rs1 == load_rd && load_rd != 0) return true;
    if (uses_rs2 && d.rs2 == load_rd && load_rd != 0) return true;

    return false;
}

// ─── ALU (pipeline-specific) ────────────────────────────────────────────

ExecResult PipelinedCPU::execute_alu(const Decoded& d, uint32_t rs1_val, uint32_t rs2_val, uint32_t pc) {
    ExecResult r = {};

    switch (d.opcode) {
    case OP_LUI:
        r.result = (uint32_t)d.imm_u;
        r.writes_rd = true;
        break;

    case OP_AUIPC:
        r.result = pc + (uint32_t)d.imm_u;
        r.writes_rd = true;
        break;

    case OP_JAL:
        r.result = pc + 4;
        r.writes_rd = true;
        r.is_branch = true;
        r.branch_taken = true;
        r.branch_target = pc + (uint32_t)d.imm_j;
        break;

    case OP_JALR:
        r.result = pc + 4;
        r.writes_rd = true;
        r.is_branch = true;
        r.branch_taken = true;
        r.branch_target = (rs1_val + (uint32_t)d.imm_i) & ~1u;
        break;

    case OP_BRANCH: {
        int32_t a = (int32_t)rs1_val, b = (int32_t)rs2_val;
        uint32_t ua = rs1_val, ub = rs2_val;
        bool taken = false;
        switch (d.funct3) {
            case 0b000: taken = (a == b); break;
            case 0b001: taken = (a != b); break;
            case 0b100: taken = (a < b); break;
            case 0b101: taken = (a >= b); break;
            case 0b110: taken = (ua < ub); break;
            case 0b111: taken = (ua >= ub); break;
        }
        r.is_branch = true;
        r.branch_taken = taken;
        r.branch_target = pc + (uint32_t)d.imm_b;
        break;
    }

    case OP_LOAD:
        r.result = rs1_val + (uint32_t)d.imm_i;
        r.writes_rd = true;
        r.is_load = true;
        break;

    case OP_STORE:
        r.result = rs1_val + (uint32_t)d.imm_s;
        r.store_value = rs2_val;
        r.is_store = true;
        break;

    case OP_IMM: {
        uint32_t src = rs1_val;
        switch (d.funct3) {
            case 0b000: r.result = src + (uint32_t)d.imm_i; break;
            case 0b010: r.result = ((int32_t)src < d.imm_i) ? 1 : 0; break;
            case 0b011: r.result = (src < (uint32_t)d.imm_i) ? 1 : 0; break;
            case 0b100: r.result = src ^ (uint32_t)d.imm_i; break;
            case 0b110: r.result = src | (uint32_t)d.imm_i; break;
            case 0b111: r.result = src & (uint32_t)d.imm_i; break;
            case 0b001: r.result = src << (d.imm_i & 0x1F); break;
            case 0b101:
                if (d.funct7 & 0x20)
                    r.result = (uint32_t)((int32_t)src >> (d.imm_i & 0x1F));
                else
                    r.result = src >> (d.imm_i & 0x1F);
                break;
        }
        r.writes_rd = true;
        break;
    }

    case OP_REG: {
        uint32_t a = rs1_val, b = rs2_val;
        switch (d.funct3) {
            case 0b000: r.result = (d.funct7 & 0x20) ? (a - b) : (a + b); break;
            case 0b001: r.result = a << (b & 0x1F); break;
            case 0b010: r.result = ((int32_t)a < (int32_t)b) ? 1 : 0; break;
            case 0b011: r.result = (a < b) ? 1 : 0; break;
            case 0b100: r.result = a ^ b; break;
            case 0b101: r.result = (d.funct7 & 0x20) ? (uint32_t)((int32_t)a >> (b & 0x1F)) : (a >> (b & 0x1F)); break;
            case 0b110: r.result = a | b; break;
            case 0b111: r.result = a & b; break;
        }
        r.writes_rd = true;
        break;
    }
    }

    return r;
}

// ─── CSR Access ─────────────────────────────────────────────────────────

uint32_t PipelinedCPU::csr_read(uint32_t addr) const {
    switch (addr) {
        case CSR_MCYCLE:    return (uint32_t)(perf_.mcycle);
        case CSR_MCYCLEH:   return (uint32_t)(perf_.mcycle >> 32);
        case CSR_MINSTRET:  return (uint32_t)(perf_.minstret);
        case CSR_MINSTRETH: return (uint32_t)(perf_.minstret >> 32);
        case CSR_MHPMCOUNTER3: return (uint32_t)(perf_.stalls_data);
        case CSR_MHPMCOUNTER4: return (uint32_t)(perf_.stalls_branch);
        default: {
            auto it = csrs_.find(addr);
            return (it != csrs_.end()) ? it->second : 0;
        }
    }
}

void PipelinedCPU::csr_write(uint32_t addr, uint32_t value) {
    csrs_[addr] = value;
}

// ─── Observation ────────────────────────────────────────────────────────

uint64_t PipelinedCPU::get_cycles() const { return perf_.mcycle; }
uint64_t PipelinedCPU::get_instret() const { return perf_.minstret; }
PerfCounters PipelinedCPU::get_perf_counters() const { return perf_; }